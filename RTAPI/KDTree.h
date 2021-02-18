#pragma once
#include "RTstruct.h"
#include "KDstruct.h"
#include <vector>

namespace KDTREE
{
	class KDnode;
	class BoundingBox;
	class splitPlane;

	// enum
	enum Event { PRIMITIVE_BEGIN, PRIMITIVE_END };
	enum Axis { X = 0, Y = 1, Z = 2, NOSPLIT = 3 };
	typedef enum { left, right, top, bottom, front, back } Rope;

	class KDTree
	{
	public:
		KDTree(int depth = 20, int primnum = 32);
		~KDTree();

		void convertSharedKDnodes(std::vector<KDNode>& kdnodes, std::vector<int>& triangle_pool);

		void buildTree(std::vector<Triangle>& triangles);
		//void traversalTree(std::vector<KDnode*>& nlist, const Ray& ray) const;

	private:
		void optimizeRopes(int& nodeid, Rope s, BoundingBox& aabb);
		void recursiveBuildTree(std::vector<Triangle>& triangles, KDnode& node, int depth);
		splitPlane findSplitSAH(std::vector<Triangle>& triangles, KDnode& node);

		KDnode* root;
		std::vector<KDnode*> nodeList;
		const int MAXDEPTH;
		const int MAXPRIMITIVE;
	};

	class BoundingBox
	{
	public:
		BoundingBox()
		{
			minb[0] = minb[1] = minb[2] = FLT_MAX;
			maxb[0] = maxb[1] = maxb[2] = FLT_MIN;
		}

		void Expand(const Triangle& tri);

		bool Inbox(const Triangle& tri);

		//bool Inbox(const Ray& ray);

		float minb[3];
		float maxb[3];

	};

	class splitPlane
	{
	public:

		float value;
		union {
			Event event;
			Axis axis;
		};

	};

	class KDnode
	{
	public:
		KDnode()
			:left(NULL), right(NULL)
		{
			memset(ropes, -1, sizeof(int) * 6);
		}

		bool isLeaf()
		{
			return (left == NULL) && (right == NULL);
		}

		void setNode(int id, int id2, BoundingBox& box, splitPlane& sp, int ropes[], bool isLeft)
		{
			this->id = id;
			this->box = box;
			memcpy(this->ropes, ropes, sizeof(int) * 6);

			int lrope, rrope;
			int *ptr;

			if (sp.axis == X)
			{
				if (!isLeft) ptr = &this->ropes[Rope::left];
				else ptr = &this->ropes[Rope::right];
			}
			else if (sp.axis == Y)
			{
				if (!isLeft) ptr = &this->ropes[Rope::top];
				else ptr = &this->ropes[Rope::bottom];
			}
			else if (sp.axis == Z)
			{
				if (!isLeft) ptr = &this->ropes[Rope::front];
				else ptr = &this->ropes[Rope::back];
			}
			else return;  // something wrong

						  //set rope id to its neighbor(id2)
			*ptr = id2;

			if (isLeft) this->box.maxb[sp.axis] = sp.value;
			else this->box.minb[sp.axis] = sp.value;
		}

		int id;
		BoundingBox box;
		splitPlane split;
		KDnode* left;
		KDnode* right;
		int ropes[6];
		std::vector<int> indexList;
	};

} // end namespace