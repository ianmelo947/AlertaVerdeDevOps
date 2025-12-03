// worker.cpp - simple OpenCL worker (uses cpp-httplib and nlohmann/json headers)
// Compile: g++ worker.cpp -o worker -lOpenCL -pthread
// Place httplib.h and json.hpp in this folder before building.

#include "httplib.h"
#include <CL/cl.h>
#include <vector>
#include <iostream>
#include <sstream>
#include "json.hpp"

using json = nlohmann::json;

std::string errorToString(cl_int err) {
    std::stringstream ss;
    ss << "OpenCL error " << err;
    return ss.str();
}

json runVecAdd(size_t N) {
    json out;
    cl_int err;

    cl_uint numPlatforms;
    err = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (err != CL_SUCCESS || numPlatforms == 0) {
        out["error"] = "No OpenCL platform found";
        return out;
    }
    std::vector<cl_platform_id> platforms(numPlatforms);
    clGetPlatformIDs(numPlatforms, platforms.data(), NULL);

    cl_platform_id platform = platforms[0];
    cl_uint numDevices = 0;
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, NULL, &numDevices);
    if (err != CL_SUCCESS || numDevices == 0) {
        out["error"] = "No OpenCL device found";
        return out;
    }
    std::vector<cl_device_id> devices(numDevices);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, numDevices, devices.data(), NULL);
    cl_device_id device = devices[0];

    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    if (err != CL_SUCCESS) { out["error"] = errorToString(err); return out; }

    cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);
    if (err != CL_SUCCESS) { out["error"] = errorToString(err); return out; }

    const char* kernelSource = R"CLC(
    __kernel void vec_add(__global const float* a, __global const float* b, __global float* c){
        int i = get_global_id(0);
        c[i] = a[i] + b[i];
    }
    )CLC";

    cl_program program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, &err);
    if (clBuildProgram(program, 1, &device, NULL, NULL, NULL) != CL_SUCCESS) {
        size_t logSize;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        std::string log(logSize, ' ');
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, &log[0], NULL);
        out["error"] = std::string("Build failed: ") + log;
        return out;
    }

    cl_kernel kernel = clCreateKernel(program, "vec_add", &err);
    if (err != CL_SUCCESS) { out["error"] = errorToString(err); return out; }

    std::vector<float> A(N), B(N), C(N);
    for (size_t i = 0; i < N; ++i) {
        A[i] = 1.0f; B[i] = 2.0f;
    }

    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, A.data(), &err);
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float)*N, B.data(), &err);
    cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(float)*N, NULL, &err);

    clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufA);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufB);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufC);

    size_t global = N;
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) { out["error"] = errorToString(err); return out; }
    clFinish(queue);

    clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, sizeof(float)*N, C.data(), 0, NULL, NULL);

    float sample = (N>0) ? C[0] : 0.0f;
    out["size"] = N;
    out["sample_result"] = sample;
    out["message"] = "vec_add concluído";

    clReleaseMemObject(bufA);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufC);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return out;
}

int main() {
    httplib::Server svr;

    svr.Post("/compute", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto js = json::parse(req.body);
            std::string op = js.value("op", "vec_add");
            size_t size = js.value("size", 1000000);

            if (op == "vec_add") {
                auto result = runVecAdd(size);
                res.set_content(result.dump(), "application/json");
            } else {
                json out; out["error"] = "op não suportada";
                res.status = 400;
                res.set_content(out.dump(), "application/json");
            }
        } catch (std::exception& e) {
            json out; out["error"] = e.what();
            res.status = 500;
            res.set_content(out.dump(), "application/json");
        }
    });

    svr.listen("0.0.0.0", 8081);
    return 0;
}
