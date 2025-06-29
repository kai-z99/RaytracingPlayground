#pragma once

#include "Hittable.h"

#include <vector>

#ifndef MAX_OBJECTS
#define MAX_OBJECTS 4096
#endif


class HittableList : public Hittable
{
public:

	__device__ HittableList() : count(0)
	{
		this->objects = new Hittable*[MAX_OBJECTS];
	}

	__device__ HittableList(Hittable* object) : count(0)
	{
		this->objects = new Hittable * [MAX_OBJECTS];

		if (count < MAX_OBJECTS)
		{
			this->objects[count++] = object;
			this->bbox = AABB(bbox, object->BoundingBox());
		}
	}

	__device__ ~HittableList()
	{
		delete[] this->objects;
	}

	__device__ void Clear()
	{
		count = 0; bbox = AABB();
	}

	__device__ void Add(Hittable* object)
	{
		if (count < MAX_OBJECTS)
		{
			this->objects[count++] = object;
			this->bbox = AABB(this->bbox, object->BoundingBox());
		}
	}

	__device__ bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const override
	{
		HitRecord tempRec;
		bool hitAnything = false;
		double closestSoFar = ray_t.max;

		for (int i = 0; i < this->count; i++)
		{
			if (objects[i]->Hit(r, Interval(ray_t.min, closestSoFar), tempRec))
			{
				hitAnything = true;
				closestSoFar = tempRec.t;
				rec = tempRec;
			}
		}

		return hitAnything;
	}

	__device__ AABB BoundingBox() const override
	{
		return this->bbox;
	}

private:
	Hittable** objects;
	AABB bbox;
	int count;
};