#pragma once

#include "Generic.h"

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
	virtual ~Hittable() {}

	virtual bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const = 0;
};