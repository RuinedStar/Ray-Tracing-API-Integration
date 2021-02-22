#include "KDTree.h"
#include"KDstruct.h"

#include <cmath>
#include <algorithm>
#include <stack>
#include <queue>

namespace KDTREE
{

	//for box surface area
	float surfaceArea(const float *whd)
	{
		float w = whd[0];
		float h = whd[1];
		float d = whd[2];
		return 2.0f * (w * h + h * d + d * w);
	}

	//for SAH eventlist sorting
	bool sortPredictSAH(const splitPlane& lh, const splitPlane& rh)
	{
		return lh.value < rh.value;
	}


	//class BoundingBox method
	void BoundingBox::Expand(const Triangle& tri)
	{
		//x
		minb[0] = std::min(tri.v0.x, std::min(tri.v1.x, std::min(tri.v2.x, minb[0])));
		maxb[0] = std::max(tri.v0.x, std::max(tri.v1.x, std::max(tri.v2.x, maxb[0])));
		//y
		minb[1] = std::min(tri.v0.y, std::min(tri.v1.y, std::min(tri.v2.y, minb[1])));
		maxb[1] = std::max(tri.v0.y, std::max(tri.v1.y, std::max(tri.v2.y, maxb[1])));
		//z
		minb[2] = std::min(tri.v0.z, std::min(tri.v1.z, std::min(tri.v2.z, minb[2])));
		maxb[2] = std::max(tri.v0.z, std::max(tri.v1.z, std::max(tri.v2.z, maxb[2])));
	}

	bool BoundingBox::Inbox(const Triangle& tri)
	{
		float tmin, tmax;
		//x
		tmin = std::min(tri.v0.x, std::min(tri.v1.x, tri.v2.x));
		tmax = std::max(tri.v0.x, std::max(tri.v1.x, tri.v2.x));
		if (tmin > maxb[0] || tmax < minb[0]) return false;
		//y
		tmin = std::min(tri.v0.y, std::min(tri.v1.y, tri.v2.y));
		tmax = std::max(tri.v0.y, std::max(tri.v1.y, tri.v2.y));
		if (tmin > maxb[1] || tmax < minb[1]) return false;
		//z
		tmin = std::min(tri.v0.z, std::min(tri.v1.z, tri.v2.z));
		tmax = std::max(tri.v0.z, std::max(tri.v1.z, tri.v2.z));
		if (tmin > maxb[2] || tmax < minb[2]) return false;

		return true;
	}

	KDTree::KDTree(int depth, int primnum)
		:MAXDEPTH(depth), MAXPRIMITIVE(primnum)
	{
	}

	KDTree::~KDTree()
	{
		for each(auto node in nodeList) delete node;
		nodeList.clear();
	}

	void KDTree::buildTree(std::vector<Triangle>& triangles)
	{
		//printf("size tri: %d\n", triangles.size());
		
		root = new KDnode;
		root->id = nodeList.size(); //give its id with last size number of nodelist start from 0
		nodeList.push_back(root);

		std::vector<int>& triList = root->indexList;

		BoundingBox& box = root->box;
		for (int i = 0; i < triangles.size(); i++)
		{
			//expand a trianlge to the bounding box
			box.Expand(triangles[i]);
			//put in bounding box
			triList.push_back(i);
		}

		recursiveBuildTree(triangles, *root, 0);

	}

	void KDTree::recursiveBuildTree(std::vector<Triangle>& triangles, KDnode& node, int depth)
	{
		//reach limit
		if (node.indexList.size() <= MAXPRIMITIVE) return;

		if(depth >= MAXDEPTH)
		{
			printf("node contain %d triangles.\n", node.indexList.size());
			printf("out of depth!");
			system("pause");
			exit(0);
		}

		splitPlane split;
		split = findSplitSAH(triangles, node);

		//if no split is better 
		if (split.axis == NOSPLIT) return;
		node.split = split;

		int lid = nodeList.size();
		int rid = nodeList.size() + 1;

		node.left = new KDnode;
		node.left->setNode(lid, rid, node.box, split, node.ropes, true);
		nodeList.push_back(node.left);

		node.right = new KDnode;
		node.right->setNode(rid, lid, node.box, split, node.ropes, false);
		nodeList.push_back(node.right);

		//pass triangles to left & right child
		for (int i = 0; i < node.indexList.size(); i++)
		{
			// in left bounding box or right or both        
		
			if (node.left->box.Inbox(triangles[node.indexList[i]]))
			{
				//left
				node.left->indexList.push_back(node.indexList[i]);
			}

			if (node.right->box.Inbox(triangles[node.indexList[i]]))
			{
				//right
				node.right->indexList.push_back(node.indexList[i]);
			}

		}

		recursiveBuildTree(triangles, *(node.left), depth + 1);
		recursiveBuildTree(triangles, *(node.right), depth + 1);
	}

	splitPlane KDTree::findSplitSAH(std::vector<Triangle>& triangles, KDnode& node)
	{
		std::vector<int>& triIndices = node.indexList;
		// 0~2, Axis x, y, z respectively
		static std::vector<splitPlane> list[3];
		list[0].clear();
		list[1].clear();
		list[2].clear();

		splitPlane temp;
		for (int i = 0; i < triIndices.size(); i++)
		{
			const Triangle& tri = triangles[triIndices[i]];
			//x
			temp.value = std::min(tri.v0.x, std::min(tri.v1.x, tri.v2.x));
			temp.event = PRIMITIVE_BEGIN;
			list[0].push_back(temp);
			temp.value = std::max(tri.v0.x, std::max(tri.v1.x, tri.v2.x));
			temp.event = PRIMITIVE_END;
			list[0].push_back(temp);
			//y
			temp.value = std::min(tri.v0.y, std::min(tri.v1.y, tri.v2.y));
			temp.event = PRIMITIVE_BEGIN;
			list[1].push_back(temp);
			temp.value = std::max(tri.v0.y, std::max(tri.v1.y, tri.v2.y));
			temp.event = PRIMITIVE_END;
			list[1].push_back(temp);
			//z
			temp.value = std::min(tri.v0.z, std::min(tri.v1.z, tri.v2.z));
			temp.event = PRIMITIVE_BEGIN;
			list[2].push_back(temp);
			temp.value = std::max(tri.v0.z, std::max(tri.v1.z, tri.v2.z));
			temp.event = PRIMITIVE_END;
			list[2].push_back(temp);

		}

		//sort each axis 0~2 x y z
		for (int i = 0; i < 3; i++)
		{
			std::sort(list[i].begin(), list[i].end(), sortPredictSAH);
		}

		//SAH, surface area width height depth
		const float* bound_max = node.box.maxb;
		const float* bound_min = node.box.minb;
		float whd[3] = { bound_max[0] - bound_min[0], bound_max[1] - bound_min[1], bound_max[2] - bound_min[2] };
		float area = surfaceArea(whd);
		float area_left, area_right;
		float tempwhd;

		//SAH, cost
		float traversal_cost = 1;
		float intersection_cost = 80;
		float best_cost = FLT_MAX;
		float original_cost = triIndices.size() * surfaceArea(whd) * intersection_cost;
		float temp_cost;

		//SAH, count
		int left, right;

		//SAH split plane
		Axis axis[3] = { X, Y, Z };
		splitPlane best_split;

		for (int i = 0; i < 3; i++)
		{
			left = 0;
			right = triIndices.size();
			std::vector<splitPlane>& eList = list[i];
			for (int j = 0; j < eList.size(); j++)
			{
				if (eList[j].event == PRIMITIVE_END) --right;

				if (eList[j].value > bound_min[i] && eList[j].value < bound_max[i])
				{
					//SAH area left right cost = Ct + ( (Nl * Sl / S) + (Nr * Sr / S) ) * Ci
					//left
					tempwhd = eList[j].value - bound_min[i];
					std::swap(tempwhd, whd[i]);
					area_left = left * surfaceArea(whd) / area;
					std::swap(tempwhd, whd[i]);

					//right
					tempwhd = bound_max[i] - eList[j].value;
					std::swap(tempwhd, whd[i]);
					area_right = right * surfaceArea(whd) / area;
					std::swap(tempwhd, whd[i]);

					//compare cost
					temp_cost = traversal_cost + (area_left + area_right) * intersection_cost;
					if (temp_cost < best_cost)
					{
						best_cost = temp_cost;
						best_split.axis = axis[i];
						best_split.value = eList[j].value;
					}
				}

				if (eList[j].event == PRIMITIVE_BEGIN) ++left;
			}
		}

		if (original_cost < best_cost)
		{
			//printf("some nodes no split!\n");
			best_split.axis = NOSPLIT;
		}

		return best_split;
	}

	void KDTree::optimizeRopes(int& nodeid, Rope s, BoundingBox& aabb)
	{
		while (!nodeList[nodeid]->isLeaf())
		{
			const KDnode *node = nodeList[nodeid];
			//lambda: return whether rope and node split plane is parallel
			bool parallel = [](const Axis& a, const Rope& b) {
				if ((a == X && (b == left || b == right)) ||
					(a == Y && (b == top || b == bottom)) ||
					(a == Z && (b == front || b == back))) return true;
				else return false;
			}(node->split.axis, s);

			if (!parallel) return;

			if (node->split.value > aabb.minb[node->split.axis]) nodeid = node->left->id;
			else if (node->split.value < aabb.maxb[node->split.axis]) nodeid = node->right->id;
			else
			{
				printf("optimizing something wrong\n");
				system("pause");
				exit(0);
			}
		}
		return;
	}

	void KDTree::convertSharedKDnodes(std::vector<KDNode>& kdnodes, std::vector<int>& triangle_pool)
	{
		KDNode temp;
		for (int i = 0; i < nodeList.size(); i++)
		{
			auto& node = nodeList[i];
			temp.stat = (node->isLeaf()) ? isLeaf : isNode;

			//if is leaf set triangle info for the node
			if (node->isLeaf())
			{
				temp.start = triangle_pool.size();  //set last id start from large collection
				temp.end = triangle_pool.size() + node->indexList.size(); //start + total size = end id
			}
			else
			{
				temp.axis = (::Axis)node->split.axis;
				temp.split = node->split.value;
				temp.child_id = { node->left->id, node->right->id };
				temp.start = -1;
				temp.end = -1;
			}

			//copy node to kdnode collection
			kdnodes.push_back(temp);

			//if is leaf, copy triangles id in the node
			if (node->isLeaf())
			{
				//move data to large triangle collection
				for (int t = 0; t < node->indexList.size(); t++)
				{
					triangle_pool.push_back(node->indexList[t]);
				}
			}

		}
	}


} //end namespace
