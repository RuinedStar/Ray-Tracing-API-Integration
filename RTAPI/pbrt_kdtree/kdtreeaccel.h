
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

#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#include <vector>
#include <memory>
#endif

#ifndef PBRT_ACCELERATORS_KDTREEACCEL_H
#define PBRT_ACCELERATORS_KDTREEACCEL_H

// accelerators/kdtreeaccel.h*
#include "../RTstruct.h"
#include "../KDstruct.h"
#include "geometry.h"

// KdTreeAccel Declarations
struct KdAccelNode;
struct BoundEdge;

class KdTreeAccel{
  public:
    // KdTreeAccel Public Methods
    KdTreeAccel(const std::vector<std::shared_ptr<Triangle>> &p,
                int isectCost = 80, int traversalCost = 1,
                float emptyBonus = 0.5, int maxPrims = 16, int maxDepth = -1);
	Bounds3f WorldBound() const { return bounds;  }
    ~KdTreeAccel();

	//convert to My CL program format
	void convertToMyKdFormat(std::vector<KDNode>& kdnodes, std::vector<int>& kdtriangles);

  private:
    // KdTreeAccel Private Methods
    void buildTree(int nodeNum, const Bounds3f &bounds,
                   const std::vector<Bounds3f> &primBounds, int *primNums,
                   int nprims, int depth,
                   const std::unique_ptr<BoundEdge[]> edges[3], int *prims0,
                   int *prims1, int badRefines = 0);

    // KdTreeAccel Private Data
    const int isectCost, traversalCost, maxPrims;
    const float emptyBonus;
    std::vector<std::shared_ptr<Triangle>> Triangles;
    std::vector<int> TriangleIndices;

    KdAccelNode *nodes;
    int nAllocedNodes, nextFreeNode;
    Bounds3f bounds;
};

struct KdToDo {
    const KdAccelNode *node;
    float tMin, tMax;
};

std::shared_ptr<KdTreeAccel> CreateKdTreeAccelerator(
    const std::vector<std::shared_ptr<Triangle>> &prims);

#endif  // PBRT_ACCELERATORS_KDTREEACCEL_H
