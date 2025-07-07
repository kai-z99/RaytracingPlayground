#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include <cstdlib>
#include "CudaHelper.h"
#define GLM_FORCE_CUDA
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

constexpr float infinity = std::numeric_limits<float>::infinity();
constexpr float pi = 3.1415926535897932385f;

inline int CeilDiv(int num, int denom)
{
	return (num + denom - 1) / denom;
}

inline float DegreesToRadians(float degrees)
{
	return degrees * pi / 180.0f;
}

inline int RandomInt(int min, int max)
{
	return min + std::rand() % (max - min + 1);
}

__device__ inline float RandomFloat(curandState& randState)
{
	return curand_uniform(&randState);
}

__device__ inline float RandomFloat(curandState& randState, float min, float max)
{
	return min + (max - min) * RandomFloat(randState);
}

__device__ inline glm::vec3 RandomOnUnitSphere(curandState& randState)
{
	float u = RandomFloat(randState);
	float v = RandomFloat(randState);

	float theta = 2.0f * pi * u;

	float z = 2.0f * v - 1.0f;

	float r = sqrt(1.0f - z*z);

	float x = r * cos(theta);
	float y = r * sin(theta);
	return glm::vec3(x,y,z);

}

__device__ inline glm::vec3 RandomVec3Positive(curandState& randState)
{
	return glm::vec3(RandomFloat(randState), RandomFloat(randState), RandomFloat(randState));
}

inline glm::vec3 RandomOnHemisphere(glm::vec3 normal)
{
	glm::vec3 v = glm::sphericalRand(1.0f);
	if (glm::dot(v, glm::normalize(normal)) > 0.0f)
	{
		return v;
	}
	else
	{
		return -v;
	}
}

__host__ __device__ inline bool NearZero(glm::vec3 v) 
{
	float e = 1e-8f;

	return std::abs(v.x) < e &&
		   std::abs(v.y) < e &&
		   std::abs(v.z) < e;
}

#include "Ray.h"
#include "Interval.h"
#include "Definitions.h"