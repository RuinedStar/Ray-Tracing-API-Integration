#pragma once

//for host
#ifndef __OPENCL_C_VERSION__
#include <glm\glm.hpp>
typedef glm::ivec2 int2;
typedef glm::vec4 float4;
//for device
#else
#endif

#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#else
#define ALIGNED_(x) __attribute__ ((packed, aligned(x)))
#endif

#define ALIGNED_TYPE(t,x) typedef t ALIGNED_(x)

typedef enum { Xaxis = 0, Yaxis, Zaxis, None } Axis;
typedef enum { isNode = 0, isLeaf } NodeStat;
typedef enum { left = 0, right, top, bottom, front, back } Rope;


#define SIZE_PER_NODE 16

ALIGNED_TYPE(struct, 16) __AABB
{
	int tID;    //triangle id
	float4 maxBound;
	float4 minBound;
} AABB;

ALIGNED_TYPE(struct, 16) __KDNode
{
	NodeStat stat;			//is leaf or node
	Axis axis;              //split axis
	float split;            //split value
	int start;				//strat triangle id 
	int end;				//end triangle id
	int2 child_id;			//s0 left, s1 right
} KDNode;
