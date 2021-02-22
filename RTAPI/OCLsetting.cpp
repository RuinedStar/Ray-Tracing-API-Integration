#include "OCLsetting.h"
#include "RTstruct.h"
#include <CL\cl_gl.h>
#include <Windows.h>
#include <fstream>

#define DEBUG_CL
#define USE_DEVICE "Intel"

OCLsetting::OCLsetting(unsigned width /* = 800 */, unsigned height /* = 600 */)
	:isInit(false), platform(NULL), device(NULL), context(NULL),
	queue(NULL), program(NULL), kernel_PathTracing(NULL)
{
	ndr[0] = width;
	ndr[1] = height;
}

void OCLsetting::CheckInit()
{
	if (isInit == false) InitCL();
}

bool OCLsetting::FindCLGLInteropDevice(std::vector<cl_platform_id>& plts)
{

	int pCount = plts.size();

	for (int i = 0; i < pCount; ++i)
	{
		char name[256];
		clGetPlatformInfo(plts[i], CL_PLATFORM_NAME, 256, name, NULL);

		std::string namestr = name;
		if (namestr.find(USE_DEVICE) == std::string::npos) continue;

		cl_uint dCount = 0;
		clGetDeviceIDs(plts[i], CL_DEVICE_TYPE_GPU, 1, &device, &dCount);
		if (dCount != 0)
		{
			platform = plts[i];
			cl_context_properties props[] = 
			{
				CL_CONTEXT_PLATFORM,(cl_context_properties)platform,
				CL_GL_CONTEXT_KHR,	(cl_context_properties)wglGetCurrentContext(),
				CL_WGL_HDC_KHR,		(cl_context_properties)wglGetCurrentDC(),
				0
			};
			printf("%s\n", name);
			context = clCreateContext(props, 1, &device, NULL, NULL, NULL);
			clGetDeviceInfo(device, CL_DEVICE_NAME, 256, name, NULL);
			printf("%s\n", name);
			return true;
		}
	}
	return false;
}

void OCLsetting::FindFirstCLDevice(std::vector<cl_platform_id>& plts)
{
	int pCount = plts.size();

	for (int i = 0; i < pCount; ++i)
	{
		//clGetPlatformInfo(plts[i], CL_PLATFORM_NAME, 256, name, NULL);

		cl_uint dCount = 0;
		clGetDeviceIDs(plts[i], CL_DEVICE_TYPE_GPU, 1, &device, &dCount);
		if (dCount != 0)
		{
			platform = plts[i];
			context = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);
			//clGetDeviceInfo(device, CL_DEVICE_NAME, 256, name, NULL);
			break;
		}
	}
}

void OCLsetting::InitCL(bool clglinterop /*= true*/)
{
	isInit = true;

	//get all platform
	cl_uint pCount;
	clGetPlatformIDs(0, nullptr, &pCount);
	std::vector<cl_platform_id> plts(pCount);
	clGetPlatformIDs(pCount, plts.data(), NULL);

	char name[256];

	bool find = false;
	
	//if clglinterop enabled, find clglinterop device
	if (clglinterop == true) find = FindCLGLInteropDevice(plts);

	//if clglinterop disable and not find clglinterop device, find first one.
	if(find == false) FindFirstCLDevice(plts);
	
#ifdef DEBUG_CL
	if (context == NULL)
	{
		printf("context failed.\n");
		system("pause");
		exit(0);
	}

#endif


#ifdef NV_CL12

	queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE , NULL);
#else
	//create command queue
	cl_queue_properties cqprop[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
	queue = clCreateCommandQueueWithProperties(context, device, cqprop, NULL);
#endif

#ifdef DEBUG_CL
	if (queue == NULL)
	{
		printf("queue failed.\n");
		system("pause");
		exit(0);
	}
#endif

	//read cl code from file
	std::ifstream ifs1("RayTracing.cl");
	std::string clcode1((std::istreambuf_iterator<char>(ifs1)), std::istreambuf_iterator<char>());
	size_t lengths[] = { clcode1.size() + 1 };
	const char* sources[] = { clcode1.data() };
	int errr;
	program = clCreateProgramWithSource(context, 1, sources, lengths, &errr);
	errr = clBuildProgram(program, 1, &device, "", NULL, NULL);

#ifdef DEBUG_CL
	char logs[2048];
	clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(char) * 2048, logs, 0);
	printf("%s\n", logs);

#endif

	//create kernel
	kernel_PathTracing_KDtree = clCreateKernel(program, "PathTracing_kdtree", NULL);

	//frame buffer
	float fill = 0.0f;
	size_t fsize = sizeof(float) * ndr[0] * ndr[1] * 4;
	frameBuf = clCreateBuffer(context, CL_MEM_READ_WRITE, fsize, NULL, NULL);
	clEnqueueFillBuffer(queue, frameBuf, &fill, sizeof(float), 0, fsize, 0, NULL, NULL);

	//sphere light buffer
	sphlBuf = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(SphereLight) * 8, NULL, NULL);
}

OCLsetting::~OCLsetting()
{
	if (frameBuf != NULL) clReleaseMemObject(frameBuf);
	if (triBuf != NULL) clReleaseMemObject(triBuf);
	if (sphlBuf != NULL) clReleaseMemObject(sphlBuf);
	if (kernel_PathTracing != NULL) clReleaseKernel(kernel_PathTracing);
	if (program != NULL) clReleaseProgram(program);
	if (queue != NULL) clReleaseCommandQueue(queue);
	if (context != NULL) clReleaseContext(context);
}