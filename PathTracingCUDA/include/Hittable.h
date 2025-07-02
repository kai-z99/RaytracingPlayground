#pragma once

#include "Generic.h"
#include "CudaHelper.h"
#include "AABB.h"

class Material;

class HitRecord
{
public:
	glm::vec3 p;
	glm::vec3 normal;
	float t;
	Material* mat;
};

class Hittable
{
public:
	__device__ virtual ~Hittable() {}

	__device__ virtual bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;

	__device__ virtual AABB BoundingBox() const = 0;
};