#pragma once

#include "Generic.h"
#include "Scene.h"


class Camera
{
public:
    glm::vec3 center;
    glm::vec3 lookAt;
    float vfov;
    int samplesPerPixel;
    int maxRayDepth;

    __device__ Camera();
    __device__ void RenderPixel(curandState& randState, const Scene& scene, unsigned int i, unsigned int j);
    __device__ void SetPixelBuffer(unsigned char* buffer);

private:
    unsigned char* pixelBuffer;
    float halfWidth, halfHeight;
    glm::vec3 up;
                    
    float aspect;
    glm::vec3 u, v, w;
    float focalLength;
        
    __device__ void Init();

    __device__ Ray GetRay(curandState& randState, int i, int j) const;

    __device__ glm::vec3 SampleSquare(curandState& randState) const;

    //moniter undos this
    __device__ inline float LinearToGamma(float linear);
    

    __device__ void WriteColor(unsigned char* pixelBuffer, int i, int j, glm::vec3 color);

    
    //__device__ glm::vec3 RayColor(curandState& randState, const Ray& r, int depth, const Hittable& world) const;
    __device__ glm::vec3 RayColorIter(curandState& randState, Ray r, int maxDepth, const Scene& scene) const;
};