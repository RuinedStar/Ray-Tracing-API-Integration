#include "PTstruct.h"
#include "KDstruct.h"

#define EPSILON 0.001f

#define VEC4(X, Y) ( (Y == 0) ? X.s0 : ( (Y == 1) ? X.s1 : (   ( Y == 2) ? X.s2 : X.s3 ) ) )

#define MAX_COUNT 48

#define PUSH_RAY(queue, ray, count) \
	queue[count++] = ray; \

#define POP_RAY(queue, ray, count) \
	ray = queue[--count]; \	


typedef enum { Origin = 0, Reflec, Refrac } RayType;	
	
typedef struct __Record
{
	uint primID;
	PrimType prim_type;
	bool isInPrim;
	int depth;
	float t;
	//float2 uv;
	int2 INTXN; //triangles & spheres test times
} Record;

typedef struct __Ray
{
	float4 ori;
	float4 dir;
	float4 revdir;  //reverse
	float4 weight;	//rgba weight
	float4 transparency;
	float4 last_prim_color;
	RayType ray_type;
	Record rec;
	
} Ray;

float4 barycentricFinder(const float4* v0, const float4* v1, const float4* v2, const float2* uv);
void stackKDtreeTraversal(float8* kdbound, global KDNode* kdnodes, global Triangle* triangles, global int* tri_list, Record* rec, const Ray* ray);
void stacklessRopesKDtreeTraversal(global KDNode* kdnodes, global Triangle* triangles, Record* rec, const Ray* ray);
bool AABBINTXN(float2* boxt, const Ray* ray, const KDNode* node, float8* bound);
bool TriINTXN(Record* rec, const Ray* ray, const Triangle* tri, uint ID);
bool SphLiINTXN(Record* rec, const Ray* ray, const SphereLight* sph, uint ID);

float4 barycentricFinder(const float4* v0, const float4* v1, const float4* v2, const float2* uv)
{
	//uv->s0 = u, uv->s1 = v
	return uv->s0 * (*v1) + uv->s1 * (*v2) + (1.0f - uv->s0 - uv->s1) * (*v0);
}


struct kdToDo
{ 
	int nodeid, tMin, tMax;
};

void stackKDtreeTraversal(float8* kdbound, global KDNode* kdnodes, global Triangle* triangles, global int* tri_list, Record* rec, const Ray* ray)
{
	//test intersection
	float2 t_entry_exit;

	//current box intersection record
	KDNode node;
	int nodeID = 0;
	node = kdnodes[nodeID];
	float tPlane;

	bool hit = AABBINTXN(&t_entry_exit, ray, &node, kdbound);
	if(hit == false) return;

	//stack record node id
	struct kdToDo todo[64];
	int todoPos = 0;
	int firstchild, secondchild;

	hit = false;

	while(nodeID != -1)
	{

		if(rec->t < t_entry_exit.s0) break;

		node = kdnodes[nodeID];

		//hit & is node, continue track child
		if(node.stat == isNode)
		{
			float oriv = VEC4(ray->ori , node.axis);
			float dir = VEC4(ray->dir , node.axis);
			float revd = VEC4(ray->revdir , node.axis);

			tPlane = (node.split - oriv ) * revd;
			
			int belowfirst = 
				(oriv < node.split) || 
			    (oriv == node.split && dir <= 0);

			if(belowfirst)
			{	
				firstchild = node.child_id.s0;
				secondchild = node.child_id.s1;
			}
			else  
			{
				firstchild = node.child_id.s1;
				secondchild = node.child_id.s0;
			}

			//advance to next node, possibly enqueue other child
			if(tPlane > t_entry_exit.s1 || tPlane <= 0)
			{	//tPlane > tEntry_max or tPlane <= 0
				nodeID = firstchild;
			}
			else if(tPlane < t_entry_exit.s0)
			{	//tPlane < tEntry_min
				nodeID = secondchild;
			}
			else //both node need to be add, firstnode compute first
			{
				todo[todoPos].nodeid = secondchild;
				todo[todoPos].tMin = tPlane;
				todo[todoPos].tMax = t_entry_exit.s1;
				++todoPos;

				nodeID = firstchild;
				t_entry_exit.s1 = tPlane;
			}
		}
		else  //isleaf, intersect triangles in the node
		{
			Triangle tri;
			int tid;
			hit = false;
			for(int i = node.start;i < node.end;++i)
			{
				tid = tri_list[i];
				tri = triangles[tid];
				hit |= TriINTXN(rec, ray, &tri, tid);
				rec->INTXN.s0 += 1;
			}

			if(todoPos > 0)
			{
				--todoPos;
				nodeID = todo[todoPos].nodeid;
				t_entry_exit = (float2)(todo[todoPos].tMin, todo[todoPos].tMax);
			}
			else break;
		}

	}

}

void stacklessRopesKDtreeTraversal(global KDNode* kdnodes, global Triangle* triangles, Record* rec, const Ray* ray)
{
	return;
}

bool AABBINTXN(float2* boxt, const Ray* ray, const KDNode* node, float8* bound)
{
	float tmin, tmax, t_ori, tentry;
	float3 t1, t2;

	float4 minBound = bound->s0123;
	float4 maxBound = bound->s4567;

	t1 = (minBound.xyz - ray->ori.xyz) * ray->revdir.xyz;
	t2 = (maxBound.xyz - ray->ori.xyz) * ray->revdir.xyz;

	//x
	tmin = min(t1.x, t2.x);
	tmax = max(t1.x, t2.x);
	//y
	tmin = max(tmin, min(t1.y, t2.y));
	tmax = min(tmax, max(t1.y, t2.y));
	//z
	tmin = max(tmin, min(t1.z, t2.z));
	tmax = min(tmax, max(t1.z, t2.z));

	boxt->s0 = tmin;
	boxt->s1 = tmax;

	return tmax > tmin;
}

// Triangle Intersection
bool TriINTXN(Record* rec, const Ray* ray, const Triangle* tri, uint ID)
{
	if(rec->primID == ID) return false;

	float4 e1, e2, P, Q, T;
	float det, tt, uu, vv, inv_det;
	e1 = tri->v1 - tri->v0;
	e2 = tri->v2 - tri->v0;
	P = cross(ray->dir, e2);
	det = dot(e1, P);
	//if (det > -0.000001f && det < 0.000001f) return false; // culling
	if(fabs(det) < EPSILON) return false;
	inv_det = native_recip(det);

	T = ray->ori - tri->v0;
	uu = dot(T, P) * inv_det;
	if (uu < 0.0f || uu > 1.0f) return false;

	Q = cross(T, e1);
	vv = dot(ray->dir, Q) * inv_det;
	if (vv < 0.0f || (uu + vv) > 1.0f) return false;

	tt = dot(e2, Q) * inv_det;
	if (tt > EPSILON && tt < rec->t)
	{
		rec->primID = ID;
		rec->prim_type = TRI;
		//rec->uv = (float2)(uu, vv);
		rec->t = tt;
		return true;
	}
	return false;
}

// Sphere Light Intersection
bool SphLiINTXN(Record* rec, const Ray* ray, const SphereLight* sph, uint ID)
{
	float a = dot(ray->dir, ray->dir);
	float b = 2.0f * dot(ray->dir, ray->ori - sph->ori);
	float c = dot(ray->ori - sph->ori, ray->ori - sph->ori) - sph->radius * sph->radius;
	float d = b * b - 4.0f * a * c;

	if (d < 0) return false;
	float sqrtd = sqrt(d);
	float t1 = (-b - sqrtd) * native_recip(a + a);  //native_recip(a + a) is / 2a  (divide 2a)
	float t2 = (-b + sqrtd) * native_recip(a + a);  //native_recip(a + a) is / 2a
	float tt;

	if (t1 > 0 && t1 < rec->t)
	{
		rec->primID = ID;
		rec->prim_type = LIGHT;
		rec->t = t1;
		return true;
	}
	else if (t2 > 0 && t2 < rec->t)
	{
		rec->primID = ID;
		rec->prim_type = LIGHT;
		rec->t = t2;
		return true;
	}
	else return false;
}

kernel void PathTracing_kdtree(
	Info	info,
	PinholeCamera	camera,
	write_only image2d_t frame,
	float8	nodeBound,
	global	KDNode*	kdnodes,
	global  int*  kdtri_list,
	global	Triangle*	triangles,
	global	SphereLight*	sphereLights,	
	global int2* INTXN)
{
	//testing light, no support light disable
	if(info.light_enable == false) return;
	
	//---image infomation
	uint W = get_global_id(0);
	uint H = get_global_id(1);
	uint Width = get_global_size(0);
	uint Height = get_global_size(1);
	uint offset = W + Width * H;
	INTXN[offset] = (int2)(0, 0);

	//---view point calculation
	float4 viewPoint = camera.ulViewPos + camera.dxUnit *  W - camera.dyUnit *  H;
	//---carry intensity
	float4 pixel = (float4)(0, 0, 0, 0);
	
	//---generate current ray
	Ray primary_ray;
	primary_ray.ori = camera.pos;
	primary_ray.dir = normalize(viewPoint - camera.pos);
	primary_ray.revdir = native_recip(primary_ray.dir);
	primary_ray.weight = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
	primary_ray.transparency = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
	primary_ray.last_prim_color = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
	primary_ray.ray_type = Origin;
	
	//---ray state record 
	Record* new_rec = &primary_ray.rec;
	new_rec->primID = -1;
	new_rec->prim_type = MISS;
	new_rec->isInPrim = false;
	new_rec->depth = 0;
	new_rec->t = FLT_MAX;
	
	//-------recursive ray tracing(for loop version)
	int ray_count = 0;
	Ray ray_queue[16];
	
	PUSH_RAY(ray_queue, primary_ray, ray_count);
	
	Ray current_ray;
	Record* current_rec;
	Ray shadowRay;
	Record* shade_rec = &shadowRay.rec;

	while(ray_count > 0)
	{
		POP_RAY(ray_queue, current_ray, ray_count);
		current_rec = &current_ray.rec;
		
		//find all triangles intersection
		stackKDtreeTraversal(&nodeBound, kdnodes, triangles, kdtri_list, &current_ray.rec, &current_ray);
		//find all light intersection
		for (int i = 0; i < info.pl_SIZE; ++i)
		{
			const SphereLight sphl = sphereLights[i];
			if (true == sphl.enable){
				SphLiINTXN(&current_ray.rec, &current_ray, &sphl, i);
				current_rec->INTXN.s1 += 1;
			}
		}
		
		//check hit primitive
		if (current_rec->prim_type == TRI)
		{
			Triangle tri = triangles[current_rec->primID];
			
			//get triangle normal, barycentric 
			//float4 normal = normalize(barycentricFinder(&tri.n0, &tri.n1, &tri.n2, &rec.uv));
			float4 normal = (float4)(normalize(tri.n0.xyz), 0);

			//get triangle color, barycentric 
			//float4 color = barycentricFinder(&tri.m0.color, &tri.m1.color, &tri.m2.color, &rec.uv);
			float4 color = tri.m0.color;	
			current_ray.last_prim_color = tri.m0.color;
			
			//test shadow
			bool is_in_shade = false;
			float4 acc = (float4)(0, 0, 0, 0);
			float4 hit_point = current_ray.ori + current_ray.dir * current_rec->t;
			
			//shadow, compute every light source
			for (int i = 0; i < info.pl_SIZE; ++i)
			{
				const SphereLight sphl = sphereLights[i];
				if (!sphl.enable) continue;
				
				shadowRay.dir = (float4)(normalize((sphl.ori - hit_point).xyz), 0);
				shadowRay.ori = hit_point + shadowRay.dir * EPSILON;
				shadowRay.revdir = native_recip(shadowRay.dir);
				
				shade_rec->prim_type = LIGHT;
				shade_rec->t = distance(shadowRay.ori, sphl.ori);
				shade_rec->primID = current_rec->primID;
				

				stackKDtreeTraversal(&nodeBound, kdnodes, triangles, kdtri_list, shade_rec, &shadowRay);
				if (shade_rec->prim_type != LIGHT) is_in_shade = true;

				// Calculate diffuse shading
				float dot_prod = dot(normal, shadowRay.dir);

				if(dot_prod > 0 && !is_in_shade)
				{
					acc += dot_prod * color;
				}	
			}
			
			switch(current_ray.ray_type)
			{
				case Origin:
					pixel += acc * current_ray.weight;
					break;
				case Reflec:
					pixel += acc * current_ray.last_prim_color * current_ray.weight * current_ray.transparency;
					break;
				case Refrac:
					pixel += acc * current_ray.weight * current_ray.transparency;
					break;
			}
			
			//handle reflection & refraction
			if(info.maxdepth > current_rec->depth)
			{
				if(tri.brdf_type == DIELEC)  //refraction
				{                                   
					float refrac;  //refrac_index n1 / n2
					float4 N;      //normal
					if(current_rec->isInPrim)
					{
						refrac = 1.66f;	
						N = -normal;
					}
					else
					{
						refrac = 1.0f / 1.66f;
						N = normal;						
					}

					float cosI = -dot(current_ray.dir, N);
					float cos2T = 1.0f - refrac * refrac * (1.0f - cosI * cosI);
					
					if(cos2T > 0) //equal cosT > 0
					{
						float4 new_dir = (refrac * current_ray.dir) + (refrac * cosI - sqrt(cos2T)) * N;
						Ray new_ray;
						new_ray.dir = normalize(new_dir);
						new_ray.ori = hit_point + new_ray.dir * EPSILON;
						new_ray.revdir = native_recip(new_ray.dir);
						new_ray.last_prim_color = current_ray.last_prim_color;
						new_ray.weight = current_ray.weight;
						//new_ray.transparency = current_ray.transparency * exp(color * -0.15f * (current_rec->t));
						new_ray.transparency = current_ray.transparency * 0.8f;
						new_ray.ray_type = Refrac;	
						
						Record* new_rec = &new_ray.rec;
						new_rec->primID = -1;
						new_rec->prim_type = MISS;
						new_rec->isInPrim = !current_rec->isInPrim;
						new_rec->depth = current_rec->depth + 1;
						new_rec->t = FLT_MAX;
						PUSH_RAY(ray_queue, new_ray, ray_count);						
					}
				}
				if(tri.brdf_type == MIRR)  //reflection
				{
					float4 new_dir = current_ray.dir - 2.0f * dot(current_ray.dir.xyz, tri.n0.xyz) * tri.n0;
					Ray new_ray;
					new_ray.dir = normalize(new_dir);
					new_ray.ori = hit_point + new_ray.dir * EPSILON;
					new_ray.revdir = native_recip(new_ray.dir);
					new_ray.last_prim_color = current_ray.last_prim_color;
					new_ray.weight = current_ray.weight * 0.8f;
					new_ray.transparency = current_ray.transparency;
					new_ray.ray_type = Reflec;
					
					Record* new_rec = &new_ray.rec;
					new_rec->primID = -1;
					new_rec->prim_type = MISS;
					new_rec->isInPrim = false;
					new_rec->depth = current_rec->depth + 1;
					new_rec->t = FLT_MAX;
					PUSH_RAY(ray_queue, new_ray, ray_count);
				}
			}
		}
		else if (current_rec->prim_type == LIGHT)
		{	//assign light intensity
			pixel = (float4)(1, 1, 1, 1);
		}
		else
		{
			//hit miss or other situation
			pixel = (float4)(0, 0, 0, 0);
		}
		INTXN[offset] += current_rec->INTXN;
	}
	//-------recursive ray tracing(for loop version)

	int2 coord = (int2)(W, H);
	write_imagef(frame, coord, pixel);
}
