#include <Windows.h>
#include <random>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <array>

#define GLM_SWIZZLE
#include <glm\gtc\type_ptr.hpp>
#include <glm\vec4.hpp>

#include <gl\glew.h>
#include <gl\GL.h>

#include <CL\cl_gl.h>


#include "Timer.h"
#include "RTstruct.h"
#include "KDstruct.h"
#include "OCLsetting.h"
#include "pbrt_kdtree\kdtreeaccel.h"

static cl_mem INTXN = NULL;
static std::vector<int> INTXNDATA(800 * 600 * 2);

#define LIGHT_RADIUS 1.0f

//debug tool
#define DEBUGSTRING 0
#define DEBUG(x) if (DEBUGSTRING) { std::cerr << x << std::endl; } 

typedef enum { RT_MAT_DIFFUSE, RT_MAT_DIELECTRIC, RT_MAT_MIRROR } RTenum;

static Timer timer;
static unsigned WIDTH = 800, HEIGHT = 600;
static bool isInit = false;

class RTCamera
{
public:

	RTCamera()
		:fovy(60), aspect(WIDTH / (float)HEIGHT), zNear(0), zFar(9999999),
		eye(0, 0, 0), center(0, 0, -1), up(0, 1, 0)
	{
	}

	void prepareCamera()
	{
		//find view plane x and y direction unit
		glm::vec3 vdir = glm::normalize(center - eye);
		glm::vec3 dx = glm::normalize(glm::cross(vdir, up));
		glm::vec3 dy = glm::normalize(glm::cross(dx, vdir));
		
		//calculate view plane, x and y unit length per pixel 
		dx *= (glm::tan(glm::radians(fovy * aspect / 2.0f)) * 2) / WIDTH;
		dy *= (glm::tan(glm::radians(fovy / 2.0f)) * 2) / HEIGHT;

		glm::vec3 ulpos = eye + vdir - (WIDTH / 2.0f) * dx + (HEIGHT / 2.0f) * dy;

		camera.pos = glm::vec4(eye, 0);
		camera.dxUnit = glm::vec4(dx, 0);
		camera.dyUnit = glm::vec4(dy, 0);
		camera.ulViewPos = glm::vec4(ulpos, 0);
	}

	PinholeCamera camera;
	double fovy, aspect;
	double zNear, zFar;
	glm::vec3 eye, center, up;
};

class RTPointer
{

public:

	RTPointer()
	{
		Clear();
	}

	void Clear()
	{
		size = 0;
		type = 0;
		stride = 0;
		pointer = NULL;
	}

	GLint size;
	GLenum type;
	unsigned stride;
	const char* pointer;
};

class RawBuffer
{
public:

	RawBuffer()
		:size(0), binary(NULL), target(0), BRDFtype(DIFF), BRDFrIndex(1)
	{
	}

	~RawBuffer()
	{
		if(binary != NULL) delete[] binary;
	}

	void SetData(int size, const GLvoid* data)
	{
		if (this->size < size)
		{
			if (binary != NULL)
			{
				delete binary;
			}
			binary = new char[size];
		}
		memcpy(binary, data, size);
	}
	//data size
	unsigned size;
	//opengl API raw bytes 
	char* binary;
	//bind target
	GLenum target;
	//material type
	BRDFType BRDFtype;
	//material refractive index
	float BRDFrIndex;
};


//------ ray tracing core controller
class rtCore
{
public:

	rtCore();

	~rtCore();

	//for parse raw data 
	void setColorData(glm::vec4& color, unsigned index);
	void setVertexData(glm::vec4& vertex, unsigned index);
	void setNormalData(glm::vec4& normal, unsigned index);

	//enable cap
	std::unordered_map<GLenum, bool> capability;
	//gl buffer storage
	std::vector<RawBuffer> glbuffers;

	//for now rendering data given to cl kernel
	Info info;
	RTCamera rtCam;

	//frame 
	GLuint frame_texture;
	cl_mem frame_texture_img;
	std::vector<float> frameData;         // 4 float per pixel 

	std::vector<Triangle> triangleData;
	std::array<SphereLight, 8> pointLight; //light structure fixed number 8
	bool isTreeBuild;

	//kd-tree
	std::vector<KDNode> kdnodes;
	std::vector<int> kdtriangles;
	cl_mem node_buf;
	cl_mem trilist_buf;

	//for binding buffer
	RawBuffer* bindVBO;
	RawBuffer* bindIndexVBO;

	// 3 pointer
	RTPointer vptr;  //vertex
	RTPointer cptr;  //color
	RTPointer nptr;  //normal
};

rtCore::rtCore()
	:rtCam(), isTreeBuild(false)
{
	//frameData.resize(WIDTH * HEIGHT * 4, 0.0f);  // init value 0
	
	//gl buffer , first is vbo 0 
	glbuffers.push_back(RawBuffer());
	bindVBO = &glbuffers.front();
	bindIndexVBO = &glbuffers.front();

	capability.emplace(GL_ARRAY_BUFFER, false);
	capability.emplace(GL_VERTEX_ARRAY, false);
	capability.emplace(GL_COLOR_ARRAY, false);
	capability.emplace(GL_NORMAL_ARRAY, false);
	capability.emplace(GL_COLOR_MATERIAL, false);
	capability.emplace(GL_LIGHTING, false);         //5
	capability.emplace(GL_LIGHT0, false);           //6
	capability.emplace(GL_LIGHT1, false);
	capability.emplace(GL_LIGHT2, false);
	capability.emplace(GL_LIGHT3, false);
	capability.emplace(GL_LIGHT4, false);
	capability.emplace(GL_LIGHT5, false);
	capability.emplace(GL_LIGHT6, false);
	capability.emplace(GL_LIGHT7, false);        

	//info
	info.samples = 1;
	info.tri_SIZE = 0;
	info.pl_SIZE = 8;  
	info.maxdepth = 8;

	//point light
	Material m;
	m.color = glm::vec4(1, 1, 1, 1);
	m.rIndex = 1;
	for (int i = 0; i < 8; ++i)
	{
		pointLight[i].ori = glm::vec4(0, 0, 0, 0);
		pointLight[i].radius = LIGHT_RADIUS;
		pointLight[i].mat = m;
		pointLight[i].enable = false;
	}
}

rtCore::~rtCore()
{

}

void rtCore::setColorData(glm::vec4& color, unsigned index)
{
	char* ptr = bindVBO->binary + (unsigned)cptr.pointer + cptr.stride * index;
	switch (cptr.type)
	{
	case GL_FLOAT:
		color = glm::vec4(
			*((float*)ptr),
			*((float*)ptr + 1),
			*((float*)ptr + 2), 1);
		break;
	case GL_DOUBLE:
		color = glm::vec4(
			*((double*)ptr),
			*((double*)ptr + 1),
			*((double*)ptr + 2), 1);
		break;
	default:
		break;
	}

	static glm::vec4 minc = glm::vec4(0, 0, 0, 1);
	static glm::vec4 maxc = glm::vec4(1, 1, 1, 1);
	color = glm::clamp(color, minc, maxc);
}

void rtCore::setVertexData(glm::vec4& vertex, unsigned index)
{
	char* ptr = bindVBO->binary + (unsigned)vptr.pointer + vptr.stride * index;
	switch (vptr.type)
	{
	case GL_FLOAT:
		vertex = glm::vec4(
			*((float*)ptr),
			*((float*)ptr + 1),
			*((float*)ptr + 2), 1);
		break;
	case GL_DOUBLE:
		vertex = glm::vec4(
			*((double*)ptr),
			*((double*)ptr + 1),
			*((double*)ptr + 2), 1);
		break;
	default:
		break;
	}
}

void rtCore::setNormalData(glm::vec4& normal, unsigned index)
{
	char* ptr = bindVBO->binary + (unsigned)nptr.pointer + nptr.stride * index;
	switch (nptr.type)
	{
	case GL_FLOAT:
		normal = glm::vec4(
			*((float*)ptr),
			*((float*)ptr + 1),
			*((float*)ptr + 2), 0);
		break;
	case GL_DOUBLE:
		normal = glm::vec4(
			*((double*)ptr),
			*((double*)ptr + 1),
			*((double*)ptr + 2), 0);
		break;
	default:
		break;
	}
}


// Only one core and ocl context in program
static OCLsetting Ocl(WIDTH, HEIGHT);
static rtCore Core;
//static KDTREE::KDTree kdtree(48, 16); //depth 20 , prim per node 32
static std::shared_ptr<KdTreeAccel> pbrt_kdtree;

/////////// implement plugining api  ///////////

void rtInit()
{
	if (isInit == true) return;
	isInit = true;

	//init opencl setting, and enable CL GL interop
	Ocl.InitCL(true);

	glGenTextures(1, &Core.frame_texture);
	glBindTexture(GL_TEXTURE_2D, Core.frame_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	Core.frame_texture_img = clCreateFromGLTexture(Ocl.context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, Core.frame_texture, NULL);

	INTXN = clCreateBuffer(Ocl.context, CL_MEM_READ_WRITE, sizeof(int) * 800 * 600 * 2, NULL, NULL);

}

void rtGenBuffers(GLsizei n, GLuint* buffers)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	if (n <= 0) return;
	for (unsigned i = 0; i < n; i++)
	{
		buffers[i] = Core.glbuffers.size();
		Core.glbuffers.push_back(RawBuffer());
	}
}

void rtBindBuffer(GLenum target, GLuint buffer)
{
	Ocl.CheckInit();
	static std::array<GLenum, 2> targetArray = { GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER };
	
	if (buffer > Core.glbuffers.size()) return;

	auto& result = std::find(targetArray.begin(), 
							 targetArray.end(), target);
	switch (*result)
	{
	case GL_ARRAY_BUFFER:
		Core.bindVBO = &Core.glbuffers[buffer];
		break;
	case GL_ELEMENT_ARRAY_BUFFER:
		Core.bindIndexVBO = &Core.glbuffers[buffer];
		break;
	default:
		break;
	}

}

void rtBufferData(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	static std::array<GLenum, 3> usageArray = { GL_STREAM_DRAW, GL_STATIC_DRAW, GL_DYNAMIC_DRAW };
	auto& u = std::find(usageArray.begin(), usageArray.end(), usage);

	if (size <= 0 && u == usageArray.end())
	{   
		return;
	}

	RawBuffer* rw = NULL;
	switch (target)
	{
	case GL_ARRAY_BUFFER:
		rw = (RawBuffer*)Core.bindVBO;
		break;
	case GL_ELEMENT_ARRAY_BUFFER:
		rw = (RawBuffer*)Core.bindIndexVBO;
		break;
	default:
		return; 
	}
	
	rw->SetData(size, data);
}

void rtVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	if (size != 3)
	{
		return;
	}

	unsigned st;
	switch (type)
	{
	case GL_FLOAT:
		st = sizeof(float);
		break;
	case GL_DOUBLE:
		st = sizeof(double);
		break;
	default:
		return; 
	}

	if (stride == 0) stride = st * size;

	Core.vptr.size = size;
	Core.vptr.type = type;
	Core.vptr.stride = stride;
	Core.vptr.pointer = (char*)pointer;
}

void rtColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	if (size != 3)
	{
		return;
	}

	unsigned st;
	switch (type)
	{
	case GL_FLOAT:
		st = sizeof(float);
		break;
	case GL_DOUBLE:
		st = sizeof(double);
		break;
	default:
		return; 
	}

	if (stride == 0) stride = st * size;

	Core.cptr.size = size;
	Core.cptr.type = type;
	Core.cptr.stride = stride;
	Core.cptr.pointer = (char*)pointer;
}

void rtNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	unsigned st;
	switch (type)
	{
	case GL_FLOAT:
		st = sizeof(float);
		break;
	case GL_DOUBLE:
		st = sizeof(double);
		break;
	default:
		return; 
	}

	if (stride == 0)
	{   // stride = 0, normal always 3
		stride = st * 3;
	}

	Core.nptr.size = 3;    //normal always 3
	Core.nptr.type = type;
	Core.nptr.stride = stride;
	Core.nptr.pointer = (char*)pointer;
}

void rtDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	bool vertex_CAP = Core.capability[GL_VERTEX_ARRAY];
	int size = 3;  // triangle per 3

	if (mode != GL_TRIANGLES || first < 0 ||
		count < 0  || !vertex_CAP)
	{
		return;
	}

	bool color_CAP = Core.capability[GL_COLOR_ARRAY];
	bool normal_CAP = Core.capability[GL_NORMAL_ARRAY];

	glm::mat4 modelview;
	glGetFloatv(GL_MODELVIEW_MATRIX, glm::value_ptr(modelview));

	int looptimes = count / size;
	
	//set loop times
	Core.triangleData.resize(Core.info.tri_SIZE + looptimes);

	#pragma omp parallel for
	for (int i = 0; i < looptimes; ++i)
	{
		Triangle temp;
		int j = i * 3;
		int id = Core.info.tri_SIZE + i;
		//vertex
		Core.setVertexData(temp.v0, j);
		temp.v0 = modelview * temp.v0;
		Core.setVertexData(temp.v1, j + 1);
		temp.v1 = modelview * temp.v1;
		Core.setVertexData(temp.v2, j + 2);
		temp.v2 = modelview * temp.v2;

		//material type
		temp.brdf_type = Core.bindVBO->BRDFtype;
		temp.m0.rIndex = Core.bindVBO->BRDFrIndex;
		temp.m1.rIndex = Core.bindVBO->BRDFrIndex;
		temp.m2.rIndex = Core.bindVBO->BRDFrIndex;

		if (true == color_CAP)
		{
			Core.setColorData(temp.m0.color, j);
			Core.setColorData(temp.m1.color, j + 1);
			Core.setColorData(temp.m2.color, j + 2);
		}
		else
		{
			temp.m0.color = glm::vec4(1, 1, 1, 1);
			temp.m1.color = glm::vec4(1, 1, 1, 1);
			temp.m2.color = glm::vec4(1, 1, 1, 1);
		}

		if (true == normal_CAP)
		{
			Core.setNormalData(temp.n0, j);
			temp.n0 = modelview * temp.n0;
			Core.setNormalData(temp.n1, j + 1);
			temp.n1 = modelview * temp.n1;
			Core.setNormalData(temp.n2, j + 2);
			temp.n2 = modelview * temp.n2;

		}
		else
		{
			glm::vec3 a = (temp.v1 - temp.v0).xyz();
			glm::vec3 b = (temp.v2 - temp.v0).xyz();
			temp.n0 = glm::vec4(glm::normalize(glm::cross(a, b)), 0);
			temp.n1 = temp.n0;
			temp.n2 = temp.n0;
		}

		Core.triangleData[id] = temp;
	}

	Core.info.tri_SIZE += looptimes;
}

void rtFlush()
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	Core.info.light_enable = Core.capability[GL_LIGHTING];
	unsigned ln = GL_LIGHT0;
	for (int i = 0; i < 7; i++)
	{
		Core.pointLight[i].enable = Core.capability[ln];
		ln += 1;
	}

	//check triangle data size, recreate buffer size properly
	Core.info.tri_SIZE = Core.triangleData.size();
	static unsigned TRISIZE = 0;
	if (TRISIZE < Core.info.tri_SIZE)
	{
		if (Ocl.triBuf != NULL)
		{
			clReleaseMemObject(Ocl.triBuf);
			Ocl.triBuf = NULL;
		}
		TRISIZE = Core.info.tri_SIZE;
		Ocl.triBuf = clCreateBuffer(Ocl.context, CL_MEM_READ_ONLY, sizeof(Triangle) * TRISIZE, NULL, NULL);
	}

	//send collected data to opencl buffer
	clEnqueueWriteBuffer(Ocl.queue, Ocl.triBuf, CL_FALSE, 0, sizeof(Triangle) * TRISIZE, Core.triangleData.data(), 0, NULL, NULL);
	clEnqueueWriteBuffer(Ocl.queue, Ocl.sphlBuf, CL_FALSE, 0, sizeof(SphereLight) * 8, Core.pointLight.data(), 0, NULL, NULL);
	clFinish(Ocl.queue);

	//update sample & camera
	Core.info.samples++;
	Core.rtCam.prepareCamera();

	//wait all data prepare
	clFinish(Ocl.queue);
	glFinish();

	//gain frame_texture_img usage permission
	clEnqueueAcquireGLObjects(Ocl.queue, 1,  &Core.frame_texture_img, 0, 0, NULL);

	cl_event execute_event;

	//set kernel arg
	if(Core.isTreeBuild)
	{
		Bounds3f b = pbrt_kdtree->WorldBound(); 
		cl_float8 bound = { b.pMin.x, b.pMin.y, b.pMin.z, 0, b.pMax.x, b.pMax.y, b.pMax.z, 0 };

		//use kdtree kernel
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 0, sizeof(Info), &Core.info);
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 1, sizeof(PinholeCamera), &Core.rtCam.camera);
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 2, sizeof(cl_mem), &Core.frame_texture_img);
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 3, sizeof(cl_float8), &bound);
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 4, sizeof(cl_mem), &Core.node_buf);
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 5, sizeof(cl_mem), &Core.trilist_buf);
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 6, sizeof(cl_mem), &Ocl.triBuf);
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 7, sizeof(cl_mem), &Ocl.sphlBuf);
		clSetKernelArg(Ocl.kernel_PathTracing_KDtree, 8, sizeof(cl_mem), &INTXN);
		//do draw call
		int err = clEnqueueNDRangeKernel(Ocl.queue, Ocl.kernel_PathTracing_KDtree, 2, NULL, Ocl.ndr, NULL, 0, NULL, &execute_event);
	}
	else
	{	
		//use common kernel
		clSetKernelArg(Ocl.kernel_PathTracing, 0, sizeof(Info), &Core.info);
		clSetKernelArg(Ocl.kernel_PathTracing, 1, sizeof(PinholeCamera), &Core.rtCam.camera);
		clSetKernelArg(Ocl.kernel_PathTracing, 2, sizeof(cl_mem), &Core.frame_texture_img);
		clSetKernelArg(Ocl.kernel_PathTracing, 3, sizeof(cl_mem), &Ocl.triBuf);
		clSetKernelArg(Ocl.kernel_PathTracing, 4, sizeof(cl_mem), &Ocl.sphlBuf);

		//do draw call
		clEnqueueNDRangeKernel(Ocl.queue, Ocl.kernel_PathTracing, 2, NULL, Ocl.ndr, NULL, 0, NULL, &execute_event);
	}

	clWaitForEvents(1, &execute_event);
	
	//release frame_texture_img usage permission
	clEnqueueReleaseGLObjects(Ocl.queue, 1, &Core.frame_texture_img, 0, 0, NULL);

 	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, Core.frame_texture);
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 1.0f);
	glVertex3f(-1.0f, -1.0f, 0.1f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex3f(1.0f, -1.0f, 0.1f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex3f(1.0f, 1.0f, 0.1f);

	glTexCoord2f(0.0f, 0.0f);
	glVertex3f(-1.0f, 1.0f, 0.1f);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	//clear data
	Core.triangleData.clear();
	Core.info.tri_SIZE = 0;
}

void rtEnableClientState(GLenum cap)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	auto& got = Core.capability.find(cap);
	if (got != Core.capability.end())
	{
		got->second = true;
	}
}

void rtDisableClientState(GLenum cap)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	auto& got = Core.capability.find(cap);
	if (got != Core.capability.end())
	{
		got->second = false;
	}
}

void rtEnable(GLenum cap)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	auto& got = Core.capability.find(cap);
	if (got != Core.capability.end())
	{
		got->second = true;
	}
}

void rtDisable(GLenum cap)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	auto& got = Core.capability.find(cap);
	if (got != Core.capability.end())
	{
		got->second = false;
	}
}

void rtPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	Core.rtCam.fovy = fovy;
	Core.rtCam.aspect = aspect;
	Core.rtCam.zNear = zNear;
	Core.rtCam.zFar = zFar;
}

void rtLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
	GLdouble centerX, GLdouble centerY, GLdouble centerZ,
	GLdouble upX, GLdouble upY, GLdouble upZ)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	Core.rtCam.eye = glm::vec3(eyeX, eyeY, eyeZ);
	Core.rtCam.center = glm::vec3(centerX, centerY, centerZ);
	Core.rtCam.up = glm::vec3(upX, upY, upZ);
}

void rtLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	unsigned index = light - GL_LIGHT0;
	if (index < 0 || index > 8) return;
	auto& pl = Core.pointLight[index];
	switch (pname)
	{
	case GL_POSITION:
		pl.ori = glm::vec4(params[0], params[1], params[2], 0);
		break;
	case GL_DIFFUSE:
		pl.mat.color = glm::vec4(params[0], params[1], params[2], 1);
		break;
	default:
		break;
	}
}

void rtMaterialEXT(RTenum type, float RefracIndex/* = 1*/)
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	if(Core.bindVBO != NULL)
	{
		switch (type)
		{
			case RT_MAT_DIELECTRIC:
				Core.bindVBO->BRDFtype = DIELEC;
				Core.bindVBO->BRDFrIndex = RefracIndex;
				break;
			case RT_MAT_DIFFUSE:
				Core.bindVBO->BRDFtype = DIFF;
				break;
			case RT_MAT_MIRROR:
				Core.bindVBO->BRDFtype = MIRR;
				break;
			default:
				Core.bindVBO->BRDFtype = DIFF;
				break;
		}
		
	}
}

void rtBuildKDtreeCurrentSceneEXT()
{
	if (isInit == false)
	{
		rtInit();
		Ocl.CheckInit();
	}

	if(!Core.isTreeBuild)
	{
		std::vector<std::shared_ptr<Triangle>> triangles;
		triangles.reserve(Core.info.tri_SIZE);
		for (int i = 0; i <Core.info.tri_SIZE; i++)
			triangles.push_back( std::make_shared<Triangle>(Core.triangleData[i]) );

		Timer buildtree, treeconvert;
		buildtree.start();
		pbrt_kdtree = std::make_shared<KdTreeAccel>(triangles);
		pbrt_kdtree->convertToMyKdFormat(Core.kdnodes, Core.kdtriangles);
		buildtree.stop();

		printf("tree build: %lf sec", buildtree.getElapsedTimeInSec());

		if (Core.node_buf != NULL) clReleaseMemObject(Core.node_buf);
		Core.node_buf = clCreateBuffer(Ocl.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(KDNode) * Core.kdnodes.size(), Core.kdnodes.data(), NULL);
		Core.trilist_buf = clCreateBuffer(Ocl.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(int) * Core.kdtriangles.size(), Core.kdtriangles.data(), NULL);
		Core.isTreeBuild = true;
	}

}

