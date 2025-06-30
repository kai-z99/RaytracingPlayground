#pragma once

#include "Hittable.h"
#include "Material.h"
#include "SetupHelper.h"
#include "Generic.h"

#include <iomanip>
#include <chrono>
#include "CudaHelper.h"

class Camera
{
public:
    glm::dvec3 center;
    glm::dvec3 lookAt;
    double vfov;
    int samplesPerPixel;
    int maxRayDepth;

    __device__ Camera();
    __device__ void RenderPixel(curandState& randState, const Hittable& world, unsigned int i, unsigned int j);
    __device__ void SetPixelBuffer(unsigned char* buffer);

private:
    unsigned char* pixelBuffer;
    double halfWidth, halfHeight;
    glm::dvec3 up;
                    
    float aspect;
    glm::dvec3 u, v, w;
    double focalLength;
        
    __device__ void Init();

    __device__ Ray GetRay(curandState& randState, int i, int j) const;

    __device__ glm::dvec3 SampleSquare(curandState& randState) const;

    //moniter undos this
    __device__ inline double LinearToGamma(double linear);
    

    __device__ void WriteColor(unsigned char* pixelBuffer, int i, int j, glm::dvec3 color);

    
    __device__ glm::dvec3 RayColor(curandState& randState, const Ray& r, int depth, const Hittable& world) const;
    __device__ glm::dvec3 RayColorIter(curandState& randState, Ray r, int maxDepth, const Hittable& world) const;
};