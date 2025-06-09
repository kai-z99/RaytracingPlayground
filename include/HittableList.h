#pragma once

#include "Hittable.h"

#include <vector>

class HittableList : public Hittable
{
public:
	std::vector<Hittable*> objects;

	HittableList() {}
	HittableList(Hittable* object) 
	{
		this->Add(object);
	}

	void Clear()
	{
		this->objects.clear();
	}

	void Add(Hittable* object)
	{
		this->objects.push_back(object);
	}

	bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const override
	{
		HitRecord tempRec;
		bool hitAnything = false;
		double closestSoFar = ray_t.max;

		for (Hittable* object : this->objects)
		{
			if (object->Hit(r, Interval(ray_t.min, closestSoFar), tempRec))
			{
				hitAnything = true;
				closestSoFar = tempRec.t;
				rec = tempRec;
			}
		}

		return hitAnything;
	}

};