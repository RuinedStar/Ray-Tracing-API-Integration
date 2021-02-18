
/*
    pbrt source code is Copyright(c) 1998-2015
                        Matt Pharr, Greg Humphreys, and Wenzel Jakob.

    This file is part of pbrt.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#if defined(_WIN32) || defined(_WIN64)
#define PBRT_IS_WINDOWS
#if defined(__MINGW32__)  // Defined for both 32 bit/64 bit MinGW
#define PBRT_IS_MINGW
#elif defined(_MSC_VER)
#define PBRT_IS_MSVC
#endif
#elif defined(__linux__)
#define PBRT_IS_LINUX
#elif defined(__APPLE__)
#define PBRT_IS_OSX
#elif defined(__OpenBSD__)
#define PBRT_IS_OPENBSD
#elif defined(__FreeBSD__)
#define PBRT_IS_FREEBSD
#endif

// accelerators/kdtreeaccel.cpp*

#include "kdtreeaccel.h"
#include <algorithm>
#include <memory>

#define Infinity std::numeric_limits<float>::infinity()

//copy from pbrt other file
inline int Log2Int(uint32_t v) {
#if defined(PBRT_IS_MSVC)
	unsigned long lz = 0;
	if (_BitScanReverse(&lz, v)) return lz;
	return 0;
#else
	return 31 - __builtin_clz(v);
#endif
}

// Memory Allocation Functions
void *AllocAligned(size_t size) {
#if defined(PBRT_IS_WINDOWS)
	return _aligned_malloc(size, 64);
#elif defined(PBRT_IS_OPENBSD) || defined(PBRT_IS_OSX) || defined(PBRT_IS_FREEBSD)
	void *ptr;
	if (posix_memalign(&ptr, PBRT_L1_CACHE_LINE_SIZE, size) != 0) ptr = nullptr;
	return ptr;
#else
	return memalign(PBRT_L1_CACHE_LINE_SIZE, size);
#endif
}

// Memory Declarations
#define ARENA_ALLOC(arena, Type) new ((arena).Alloc(sizeof(Type))) Type
void *AllocAligned(size_t size);
template <typename T>
T *AllocAligned(size_t count) {
	return (T *)AllocAligned(count * sizeof(T));
}

void FreeAligned(void *ptr) {
	if (!ptr) return;
#if defined(PBRT_IS_WINDOWS)
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}

// KdTreeAccel Local Declarations
struct KdAccelNode {
    // KdAccelNode Methods
    void InitLeaf(int *primNums, int np, std::vector<int> *TriangleIndices);
    void InitInterior(int axis, int ac, float s) {
        split = s;
        flags = axis;
        aboveChild |= (ac << 2);
    }
    float SplitPos() const { return split; }
    int nTriangles() const { return nPrims >> 2; }
    int SplitAxis() const { return flags & 3; }
    bool IsLeaf() const { return (flags & 3) == 3; }
    int AboveChild() const { return aboveChild >> 2; }
    union {
        float split;                 // Interior
        int oneTriangle;            // Leaf
        int TriangleIndicesOffset;  // Leaf
    };

  private:
    union {
        int flags;       // Both
        int nPrims;      // Leaf
        int aboveChild;  // Interior
    };
};

enum class EdgeType { Start, End };
struct BoundEdge {
    // BoundEdge Public Methods
    BoundEdge() {}
    BoundEdge(float t, int primNum, bool starting) : t(t), primNum(primNum) {
        type = starting ? EdgeType::Start : EdgeType::End;
    }
    float t;
    int primNum;
    EdgeType type;
};

// KdTreeAccel Method Definitions
KdTreeAccel::KdTreeAccel(const std::vector<std::shared_ptr<Triangle>> &p,
                         int isectCost, int traversalCost, float emptyBonus,
                         int maxPrims, int maxDepth)
    : isectCost(isectCost),
      traversalCost(traversalCost),
      maxPrims(maxPrims),
      emptyBonus(emptyBonus),
      Triangles(p) {
    // Build kd-tree for accelerator
    nextFreeNode = nAllocedNodes = 0;
    if (maxDepth <= 0)
        maxDepth = std::round(8 + 1.3f * Log2Int(Triangles.size()));

    // Compute bounds for kd-tree construction
    std::vector<Bounds3f> primBounds;
    primBounds.reserve(Triangles.size());
    for (const std::shared_ptr<Triangle> &prim : Triangles) {

		const Point3f &p0 = Point3f(prim->v0.x, prim->v0.y, prim->v0.z);
		const Point3f &p1 = Point3f(prim->v1.x, prim->v1.y, prim->v1.z);
		const Point3f &p2 = Point3f(prim->v2.x, prim->v2.y, prim->v2.z);

        Bounds3f b = Union(Bounds3f(p0, p1), p2);
        bounds = Union(bounds, b);
        primBounds.push_back(b);
    }

    // Allocate working memory for kd-tree construction
    std::unique_ptr<BoundEdge[]> edges[3];
    for (int i = 0; i < 3; ++i)
        edges[i].reset(new BoundEdge[2 * Triangles.size()]);
    std::unique_ptr<int[]> prims0(new int[Triangles.size()]);
    std::unique_ptr<int[]> prims1(new int[(maxDepth + 1) * Triangles.size()]);

    // Initialize _primNums_ for kd-tree construction
    std::unique_ptr<int[]> primNums(new int[Triangles.size()]);
    for (size_t i = 0; i < Triangles.size(); ++i) primNums[i] = i;

    // Start recursive construction of kd-tree
    buildTree(0, bounds, primBounds, primNums.get(), Triangles.size(),
              maxDepth, edges, prims0.get(), prims1.get());
}

void KdAccelNode::InitLeaf(int *primNums, int np,
                           std::vector<int> *TriangleIndices) {
    flags = 3;
    nPrims |= (np << 2);
    // Store Triangle ids for leaf node
    if (np == 0)
        oneTriangle = 0;
    else if (np == 1)
        oneTriangle = primNums[0];
    else {
        TriangleIndicesOffset = TriangleIndices->size();
        for (int i = 0; i < np; ++i) TriangleIndices->push_back(primNums[i]);
    }
}

KdTreeAccel::~KdTreeAccel() { FreeAligned(nodes); }

void KdTreeAccel::buildTree(int nodeNum, const Bounds3f &nodeBounds,
                            const std::vector<Bounds3f> &allPrimBounds,
                            int *primNums, int nTriangles, int depth,
                            const std::unique_ptr<BoundEdge[]> edges[3],
                            int *prims0, int *prims1, int badRefines) {
    //Assert(nodeNum == nextFreeNode);
    // Get next free node from _nodes_ array
    if (nextFreeNode == nAllocedNodes) {
        int nNewAllocNodes = std::max(2 * nAllocedNodes, 512);
        KdAccelNode *n = AllocAligned<KdAccelNode>(nNewAllocNodes);
        if (nAllocedNodes > 0) {
            memcpy(n, nodes, nAllocedNodes * sizeof(KdAccelNode));
            FreeAligned(nodes);
        }
        nodes = n;
        nAllocedNodes = nNewAllocNodes;
    }
    ++nextFreeNode;

    // Initialize leaf node if termination criteria met
    if (nTriangles <= maxPrims || depth == 0) {
        nodes[nodeNum].InitLeaf(primNums, nTriangles, &TriangleIndices);
        return;
    }

    // Initialize interior node and continue recursion

    // Choose split axis position for interior node
    int bestAxis = -1, bestOffset = -1;
	float bestCost = FLT_MAX;
    float oldCost = isectCost * float(nTriangles);
    float totalSA = nodeBounds.SurfaceArea();
    float invTotalSA = 1 / totalSA;
    Vector3f d = nodeBounds.pMax - nodeBounds.pMin;

    // Choose which axis to split along
    int axis = nodeBounds.MaximumExtent();
    int retries = 0;
retrySplit:

    // Initialize edges for _axis_
    for (int i = 0; i < nTriangles; ++i) {
        int pn = primNums[i];
        const Bounds3f &bounds = allPrimBounds[pn];
        edges[axis][2 * i] = BoundEdge(bounds.pMin[axis], pn, true);
        edges[axis][2 * i + 1] = BoundEdge(bounds.pMax[axis], pn, false);
    }

    // Sort _edges_ for _axis_
    std::sort(&edges[axis][0], &edges[axis][2 * nTriangles],
              [](const BoundEdge &e0, const BoundEdge &e1) -> bool {
                  if (e0.t == e1.t)
                      return (int)e0.type < (int)e1.type;
                  else
                      return e0.t < e1.t;
              });

    // Compute cost of all splits for _axis_ to find best
    int nBelow = 0, nAbove = nTriangles;
    for (int i = 0; i < 2 * nTriangles; ++i) {
        if (edges[axis][i].type == EdgeType::End) --nAbove;
        float edgeT = edges[axis][i].t;
        if (edgeT > nodeBounds.pMin[axis] && edgeT < nodeBounds.pMax[axis]) {
            // Compute cost for split at _i_th edge

            // Compute child surface areas for split at _edgeT_
            int otherAxis0 = (axis + 1) % 3, otherAxis1 = (axis + 2) % 3;
            float belowSA = 2 * (d[otherAxis0] * d[otherAxis1] +
                                 (edgeT - nodeBounds.pMin[axis]) *
                                     (d[otherAxis0] + d[otherAxis1]));
            float aboveSA = 2 * (d[otherAxis0] * d[otherAxis1] +
                                 (nodeBounds.pMax[axis] - edgeT) *
                                     (d[otherAxis0] + d[otherAxis1]));
            float pBelow = belowSA * invTotalSA;
            float pAbove = aboveSA * invTotalSA;
            float eb = (nAbove == 0 || nBelow == 0) ? emptyBonus : 0;
            float cost =
                traversalCost +
                isectCost * (1 - eb) * (pBelow * nBelow + pAbove * nAbove);

            // Update best split if this is lowest cost so far
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestOffset = i;
            }
        }
        if (edges[axis][i].type == EdgeType::Start) ++nBelow;
    }
    //Assert(nBelow == nTriangles && nAbove == 0);

    // Create leaf if no good splits were found
    if (bestAxis == -1 && retries < 2) {
        ++retries;
        axis = (axis + 1) % 3;
        goto retrySplit;
    }
    if (bestCost > oldCost) ++badRefines;
    if ((bestCost > 4 * oldCost && nTriangles < 16) || bestAxis == -1 ||
        badRefines == 3) {
        nodes[nodeNum].InitLeaf(primNums, nTriangles, &TriangleIndices);
        return;
    }

    // Classify Triangles with respect to split
    int n0 = 0, n1 = 0;
    for (int i = 0; i < bestOffset; ++i)
        if (edges[bestAxis][i].type == EdgeType::Start)
            prims0[n0++] = edges[bestAxis][i].primNum;
    for (int i = bestOffset + 1; i < 2 * nTriangles; ++i)
        if (edges[bestAxis][i].type == EdgeType::End)
            prims1[n1++] = edges[bestAxis][i].primNum;

    // Recursively initialize children nodes
    float tSplit = edges[bestAxis][bestOffset].t;
    Bounds3f bounds0 = nodeBounds, bounds1 = nodeBounds;
    bounds0.pMax[bestAxis] = bounds1.pMin[bestAxis] = tSplit;
    buildTree(nodeNum + 1, bounds0, allPrimBounds, prims0, n0, depth - 1, edges,
              prims0, prims1 + nTriangles, badRefines);
    int aboveChild = nextFreeNode;
    nodes[nodeNum].InitInterior(bestAxis, aboveChild, tSplit);
    buildTree(aboveChild, bounds1, allPrimBounds, prims1, n1, depth - 1, edges,
              prims0, prims1 + nTriangles, badRefines);
}

std::shared_ptr<KdTreeAccel> CreateKdTreeAccelerator(
    const std::vector<std::shared_ptr<Triangle>> &prims) {

    return std::make_shared<KdTreeAccel>(prims);
}

void KdTreeAccel::convertToMyKdFormat(std::vector<KDNode>& kdnodes, std::vector<int>& kdtriangles)
{
	kdnodes.clear();
	kdnodes.reserve(nextFreeNode);
	kdtriangles = TriangleIndices;

	KDNode mynode;
	for (int i = 0; i < nextFreeNode;i++)
	{
		auto& node = nodes[i];
		mynode.stat = (node.IsLeaf()) ? isLeaf : isNode;

		//if is leaf set triangle info for the node
		if (node.IsLeaf())
		{
			mynode.axis = None;
			mynode.split = 0;
			mynode.child_id = { -1, -1 };	
			mynode.start = node.TriangleIndicesOffset;
			mynode.end = node.TriangleIndicesOffset + node.nTriangles();
		}
		else
		{
			mynode.axis = (::Axis)node.SplitAxis();
			mynode.split = node.SplitPos();
			mynode.child_id = { i + 1, node.AboveChild() };  //left child is current node id + 1
			mynode.start = -1;
			mynode.end = -1;
		}
		kdnodes.push_back(mynode);
	}
}
