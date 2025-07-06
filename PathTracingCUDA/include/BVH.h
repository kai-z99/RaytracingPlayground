#pragma once

#include "AABB.h"
#include "Hittable.h"
#include "HittableList.h"

//Naive: ~1700 seconds
//Random: 137.1 seconds
//Longest Axis: 62.3 seconds


//class BVHNode : public Hittable
//{
//public:
//	__device__ BVHNode(HittableList& list) : BVHNode(list.objects, 0, list.count) {}
//
//	__device__ BVHNode(Hittable** objects, int start, int end)
//	{
//		//empty
//		this->bbox = AABB(Interval(+infinity,-infinity), Interval(+infinity, -infinity), Interval(+infinity, -infinity));
//		for (int idx = start; idx < end; idx++)
//		{
//			this->bbox = AABB(this->bbox, objects[idx]->BoundingBox()); //expand
//		}
//
//		int axis = this->bbox.LongestAxis();
//		int objectSpan = end - start;
//
//		if (objectSpan == 1)
//		{
//			left = objects[start];
//			right = objects[start];
//		}
//		else if (objectSpan == 2)
//		{
//			left = objects[start];
//			right = objects[start + 1];
//		}
//		else
//		{
//			SortRange(objects, start, end, axis);
//			int mid = start + (objectSpan / 2);
//			left = new BVHNode(objects, start, mid);
//			right = new BVHNode(objects, mid, end);
//		}
//	}
//
//	__device__ bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const override
//	{
//		if (!this->bbox.Hit(r, ray_t))
//		{
//			return false;
//		}
//
//		bool hitLeft = left->Hit(r, ray_t, rec);
//		//if hit left, the new max is <= ray_t.max
//		bool hitRight = right->Hit(r, Interval(ray_t.min, hitLeft? rec.t : ray_t.max), rec);
//
//		return hitLeft || hitRight;
//	}
//
//	__device__ AABB BoundingBox() const override
//	{
//		return this->bbox;
//	}
//
//private:
//	Hittable* left;
//	Hittable* right;
//	AABB bbox;
//
//	__device__ static void SortRange(Hittable** objects, int start, int end, int axis)
//	{
//		for (int i = start; i < end - 1; ++i)
//		{
//			int    minIdx = i;
//			double minVal = objects[i]->BoundingBox().AxisInterval(axis).min;
//
//			for (int j = i + 1; j < end; ++j)
//			{
//				double val = objects[j]->BoundingBox().AxisInterval(axis).min;
//				if (val < minVal) { minVal = val; minIdx = j; }
//			}
//			if (minIdx != i)
//			{
//				Hittable* tmp = objects[i];
//				objects[i] = objects[minIdx];
//				objects[minIdx] = tmp;
//			}
//		}
//	}
//
//};




//REFACTOR

enum PrimType
{
	PRIM_SPHERE = 0,
	PRIM_QUAD = 1,
};

struct BVHNode
{
	glm::vec3 bboxMin;
	glm::vec3 bboxMax;
	uint32_t leftFirst; //INNER: index of left node in BVHNodes* (right is +1) . LEAF: index of first prim in primtypes*/indices*
	uint32_t primCount; //0 means inner node, >0 means leaf node. (Note we dont always have 1 prim per leaf as the cost of traversing will be higher than collision detection)

	//the goal is that when the traversel hits a leaf, it refers you to a spot in the parralell arrays primIndices* and primTypes* to do the collision.
	//Note that prims in the same leaf will ALWAYS be contigous in the paralell arrays. This is why primCount will simply let you traverse forward 
	// n times and hit all the prims in the leaf.
};

//first implement longest axis bvh build
//then implement binned SAH

//we can safely pass in scene->BVHNodes, scene->nodeCount.. etc to this function.
//We can also pass in SpherePacked and QuadsPacked that were malloced by cudaMallocManaged. perfect.
inline void BuildBVH(
	const SpheresPacked& spheres,
	const QuadsPacked& quads,
	BVHNode*& outNodes,
	uint32_t& outNodeCount,
	PrimType*& outPrimTypes,
	uint32_t*& outPrimIndices)
{

}

