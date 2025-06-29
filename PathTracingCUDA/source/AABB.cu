#pragma once

#include "../include/AABB.h"
#include "../include/Interval.h"

__device__ AABB::AABB() {}

__device__ AABB::AABB(const Interval& x, const Interval& y, const Interval& z)
		: x(x), y(y), z(z) {}

__device__ AABB::AABB(const glm::dvec3& a, const glm::dvec3& b)
{
	this->x = (a[0] <= b[0]) ? Interval(a[0], b[0]) : Interval(b[0], a[0]);
	this->y = (a[1] <= b[1]) ? Interval(a[1], b[1]) : Interval(b[1], a[1]);
	this->z = (a[2] <= b[2]) ? Interval(a[2], b[2]) : Interval(b[2], a[2]);
}

__device__ AABB::AABB(const AABB& box0, const AABB& box1)
{
	this->x = Interval(box0.x, box1.x);
	this->y = Interval(box0.y, box1.y);
	this->z = Interval(box0.z, box1.z);
}

__device__ const Interval& AABB::AxisInterval(int n) const
{
	if (n == 1) return this->y;
	if (n == 2) return this->z;
	return this->x;
}

__device__ bool AABB::Hit(const Ray& r, Interval ray_t) const
{
	const glm::dvec3& rayOrigin = r.origin();
	const glm::dvec3& rayDirection = r.direction();

	for (int axis = 0; axis < 3; axis++)
	{
		const Interval& ax = AxisInterval(axis); //x,y,z

		//solve t0 = (x_0 - Q_x) / d_x (for x,y,z)
		const double adInverse = 1.0 / rayDirection[axis];
		double t0 = (ax.min - rayOrigin[axis]) * adInverse;
		double t1 = (ax.max - rayOrigin[axis]) * adInverse;

		//shrink ray interval to just the box axis
		if (t0 < t1)
		{
			if (t0 > ray_t.min) ray_t.min = t0;
			if (t1 < ray_t.max) ray_t.max = t1;
		}
		else
		{
			if (t1 > ray_t.min) ray_t.min = t1;
			if (t0 < ray_t.max) ray_t.max = t0;
		}

		if (ray_t.max <= ray_t.min)
		{
			return false;
		}
	}

	return true;
}

__device__ int AABB::LongestAxis() const
{
	if (x.Size() > y.Size())
	{
		return x.Size() > z.Size() ? 0 : 2;
	}
	else
	{
		return y.Size() > z.Size() ? 1 : 2;
	}
}

static const AABB empty, universe;

const AABB AABB::empty = AABB(Interval::Empty, Interval::Empty, Interval::Empty);
const AABB AABB::universe = AABB(Interval::Universe, Interval::Universe, Interval::Universe);