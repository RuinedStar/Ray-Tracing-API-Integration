#pragma once

#include <vector>
#include <CL\cl.h>

class OCLsetting
{
private:
	bool FindCLGLInteropDevice(std::vector<cl_platform_id>& plts);
	void FindFirstCLDevice(std::vector<cl_platform_id>& plts);

public:

	OCLsetting(unsigned width = 800, unsigned height = 600);
	~OCLsetting();

	void InitCL(bool clglinterop = true);
	void CheckInit();

	cl_platform_id platform;
	cl_device_id device;
	cl_context context;
	cl_command_queue queue;
	cl_program program;
	cl_kernel kernel_PathTracing;
	cl_kernel kernel_PathTracing_KDtree;
	//kernel ndrange
	size_t ndr[2];

	//check if init all cl setting
	bool isInit;

	// cl buffer or image for kernel
	cl_mem frameBuf, triBuf, sphlBuf, kdtriBuf;

};