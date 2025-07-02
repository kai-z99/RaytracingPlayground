#pragma once

#include "Interval.h"

class AABB
{
public:
	Interval x, y, z;

	__device__ AABB();

	__device__ AABB(const Interval& x, const Interval& y, const Interval& z);

	__device__ AABB(const glm::vec3& a, const glm::vec3& b);

	__device__ AABB(const AABB& box0, const AABB& box1);

	__device__ const Interval& AxisInterval(int n) const;

	__device__ bool Hit(const Ray& r, Interval ray_t) const;

	__device__ int LongestAxis() const;

	static const AABB empty, universe;
};