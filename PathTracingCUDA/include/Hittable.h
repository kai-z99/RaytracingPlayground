#pragma once

#include "Generic.h"
#include "CudaHelper.h"
#include "AABB.h"

class Material;

class HitRecord
{
public:
	glm::dvec3 p;
	glm::dvec3 normal;
	double t;
	Material* mat;
};

class Hittable
{
public:
	__device__ virtual ~Hittable() {}

	__device__ virtual bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;

	__device__ virtual AABB BoundingBox() const = 0;
};