#pragma once

#include "AABB.h"
#include "Hittable.h"
#include "HittableList.h"

#include <algorithm>

//Naive: ~1700 seconds
//Random: 137.1 seconds
//Longest Axis: 62.3 seconds

class BVHNode : public Hittable
{
public:
	BVHNode(HittableList& list) : BVHNode(list.objects, 0, list.objects.size()) {}

	BVHNode(std::vector<Hittable*>& objects, size_t start, size_t end)
	{
		this->bbox = AABB::empty;
		for (size_t idx = start; idx < end; idx++)
		{
			this->bbox = AABB(this->bbox, objects[idx]->BoundingBox());
		}

		int axis = this->bbox.LongestAxis();

		auto comparator = (axis == 0) ? boxCompareX
			: (axis == 1) ? boxCompareY : boxCompareZ;

		size_t objectSpan = end - start;

		if (objectSpan == 1)
		{
			left = objects[start];
			right = objects[start];
		}
		else if (objectSpan == 2)
		{
			left = objects[start];
			right = objects[start + 1];
		}
		else
		{
			std::sort(std::begin(objects) + start, std::begin(objects) + end, comparator);
			size_t mid = start + (objectSpan / 2);
			left = new BVHNode(objects, start, mid);
			right = new BVHNode(objects, mid, end);
		}
	}

	bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const override
	{
		if (!this->bbox.Hit(r, ray_t))
		{
			return false;
		}

		bool hitLeft = left->Hit(r, ray_t, rec);
		//if hit left, the new max is <= ray_t.max
		bool hitRight = right->Hit(r, Interval(ray_t.min, hitLeft? rec.t : ray_t.max), rec);

		return hitLeft || hitRight;
	}

	AABB BoundingBox() const override 
	{
		return this->bbox;
	}

private:
	Hittable* left;
	Hittable* right;
	AABB bbox;

	static bool boxCompare(const Hittable* a, const Hittable* b, int axisIndex)
	{
		Interval aAxisInterval = a->BoundingBox().AxisInterval(axisIndex);
		Interval bAxisInterval = b->BoundingBox().AxisInterval(axisIndex);
		return aAxisInterval.min < bAxisInterval.min;

	}

	static bool boxCompareX(const Hittable* a, const Hittable* b) 
	{
		return boxCompare(a, b, 0);
	}

	static bool boxCompareY(const Hittable* a, const Hittable* b) 
	{
		return boxCompare(a, b, 1);
	}

	static bool boxCompareZ(const Hittable* a, const Hittable* b) 
	{
		return boxCompare(a, b, 2);
	}


};

