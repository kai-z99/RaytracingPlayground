#pragma once

#include "AABB.h"
#include "Hittable.h"
#include "HittableList.h"

//Naive: ~1700 seconds
//Random: 137.1 seconds
//Longest Axis: 62.3 seconds


class BVHNode : public Hittable
{
public:
	__device__ BVHNode(HittableList& list) : BVHNode(list.objects, 0, list.count) {}

	__device__ BVHNode(Hittable** objects, int start, int end)
	{
		//empty
		this->bbox = AABB(Interval(+infinity,-infinity), Interval(+infinity, -infinity), Interval(+infinity, -infinity));
		for (int idx = start; idx < end; idx++)
		{
			this->bbox = AABB(this->bbox, objects[idx]->BoundingBox()); //expand
		}

		int axis = this->bbox.LongestAxis();
		int objectSpan = end - start;

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
			SortRange(objects, start, end, axis);
			int mid = start + (objectSpan / 2);
			left = new BVHNode(objects, start, mid);
			right = new BVHNode(objects, mid, end);
		}
	}

	__device__ bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const override
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

	__device__ AABB BoundingBox() const override
	{
		return this->bbox;
	}

private:
	Hittable* left;
	Hittable* right;
	AABB bbox;

	__device__ static void SortRange(Hittable** objects, int start, int end, int axis)
	{
		for (int i = start; i < end - 1; ++i)
		{
			int    minIdx = i;
			double minVal = objects[i]->BoundingBox().AxisInterval(axis).min;

			for (int j = i + 1; j < end; ++j)
			{
				double val = objects[j]->BoundingBox().AxisInterval(axis).min;
				if (val < minVal) { minVal = val; minIdx = j; }
			}
			if (minIdx != i)
			{
				Hittable* tmp = objects[i];
				objects[i] = objects[minIdx];
				objects[minIdx] = tmp;
			}
		}
	}

};

