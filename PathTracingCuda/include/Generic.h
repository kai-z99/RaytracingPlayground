#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include <cstdlib>
#include "CudaHelper.h"
#define GLM_FORCE_CUDA
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

__device__ constexpr double infinity = std::numeric_limits<double>::infinity();
constexpr double pi = 3.1415926535897932385;

inline int CeilDiv(int num, int denom)
{
	return (num + denom - 1) / denom;
}


inline double DegreesToRadians(double degrees)
{
	return degrees * pi / 180.0;
}

inline int RandomInt(int min, int max)
{
	return min + std::rand() % (max - min + 1);
}

inline double RandomDouble()
{
	return std::rand() / (RAND_MAX + 1.0);
}

inline double RandomDouble(double min, double max)
{
	return min + (max - min) * RandomDouble();
}

inline glm::dvec3 RandomOnHemisphere(glm::dvec3 normal)
{
	glm::dvec3 v = glm::sphericalRand(1.0f);
	if (glm::dot(v, glm::normalize(normal)) > 0.0)
	{
		return v;
	}
	else
	{
		return -v;
	}
}

__device__ inline bool NearZero(glm::dvec3 v) 
{
	double e = 1e-8;

	return std::abs(v.x) < e &&
		   std::abs(v.y) < e &&
		   std::abs(v.z) < e;
}



#include "Ray.h"
#include "Interval.h"
#include "Definitions.h"