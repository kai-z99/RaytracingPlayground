#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <glm/gtc/random.hpp>

const double infinity = std::numeric_limits<double>::infinity();
const double pi = 3.1415926535897932385;


inline double DegreesToRadians(double degrees)
{
	return degrees * pi / 180.0;
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

inline bool NearZero(glm::dvec3 v) 
{
	double e = 1e-8;

	return std::abs(v.x) < e &&
		   std::abs(v.y) < e &&
		   std::abs(v.z) < e;
}



#include "Ray.h"
#include "Interval.h"
#include "Definitions.h"