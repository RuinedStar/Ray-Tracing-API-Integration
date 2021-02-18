#pragma once

//emissive, diffuse, DIELEC, mirror
typedef enum { EMIS, DIFF, DIELEC, MIRR } BRDFType;

//for host
#ifndef __OPENCL_C_VERSION__
	#include <glm\glm.hpp>
	typedef glm::vec4 float4;
//for device
#else
	typedef enum { MISS, LIGHT, TRI, SPH } PrimType;
#endif

#if defined(_MSC_VER)
#define _CL_ALIGNED_(x) alignas(x)
#else 
#define _CL_ALIGNED_(x) 
#endif

#define CL_VEC4_ALIGN _CL_ALIGNED_(16)

typedef struct PinholeCamera
{
	CL_VEC4_ALIGN float4 pos;
	CL_VEC4_ALIGN float4 ulViewPos;	//upper left corner on rendering image
	CL_VEC4_ALIGN float4 dxUnit;
	CL_VEC4_ALIGN float4 dyUnit;
} PinholeCamera;

typedef struct __Info
{
	int tri_SIZE;      //triangle size
	int pl_SIZE;       //point light
	int samples;
	int maxdepth;
	int light_enable;  //1 enable, 0 disable
} Info;

typedef struct __Material
{
	CL_VEC4_ALIGN float4 color;
	float rIndex;      //refractive index
} Material;

typedef struct __Triangle
{
	BRDFType brdf_type;  //emissive, diffuse, dielec, mirror
	CL_VEC4_ALIGN float4 v0, v1, v2;  //vertex
	CL_VEC4_ALIGN float4 n0, n1, n2;  //normal
	Material m0, m1, m2;
} Triangle;

typedef struct __Sphere
{
	CL_VEC4_ALIGN float4 ori;
	float  radius;
	Material mat;
} Sphere;

typedef struct __SphereLight
{
	CL_VEC4_ALIGN float4 ori;
	Material mat;
	float  radius;
	int enable;
} SphereLight;


