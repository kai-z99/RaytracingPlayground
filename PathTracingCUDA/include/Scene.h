#pragma once

#include "Generic.h"
#include "Material.h"
#include "Primitives.h"
#include "BVH.h"
#include "AABB.h"

//device only
struct Scene
{
    SpheresPacked* spheres;
    QuadsPacked* quads;
    TrianglesPacked* tris;

    BVHNode* BVHNodes;
    uint32_t BVHCount;
    //how the bvh accesses the SoAs
    PrimType* primTypes;    //type
    uint32_t* primIndices;  //index in the corresponding AoS
    
    MaterialData* materials;
    uint32_t materialCount;
};

//traverse the bvh instead of linearly scanning
 __device__ inline bool HitScene(const Scene& scene, const Ray& r, Interval ray_t, HitRecord& rec)
{
     constexpr int MAX_STACK = 64;
     uint32_t stack[MAX_STACK]; //each element represents the index in BVHNodes*
     int stackPtr = 0;
     bool hitAny = false;
     float closestSoFar = ray_t.max;

     //start with the root of BVH
     stack[stackPtr++] = 0;

     while (stackPtr) //while stack ptr is not at 0. (0 is bottom of stack)
     {
         uint32_t nodeIdx = stack[--stackPtr]; //pop the top of stack
         const BVHNode& node = scene.BVHNodes[nodeIdx];

         //if that bvh node's AABB doesnt intersect with ray, just conitnue and pop the next element.
         if (IntersectAABB(r, node.bboxMin, node.bboxMax, closestSoFar) < 0.0f) //miss?
         {
             continue;
         }

         //The ray has intersected the AABB.

         //is it a leaf?
         if (node.primCount)
         {
             //go through and test each prim in that leaf
             for (uint32_t i = 0; i < node.primCount; i++)
             {
                 //index in parallel arrays
                 uint32_t index = node.leftFirst + i;
                 PrimType type = scene.primTypes[index];
                 uint32_t indexInAoS = scene.primIndices[index];

                 HitRecord tempRec;
                 bool hit = false;

                 switch (type)
                 {
                 case PRIM_SPHERE:
                 {
                     hit = HitSphere(*scene.spheres, indexInAoS, r, Interval(ray_t.min, closestSoFar), tempRec);
                     break;
                 }
                 case PRIM_QUAD:
                 {
                     hit = HitQuad(*scene.quads, indexInAoS, r, Interval(ray_t.min, closestSoFar), tempRec);
                     break;
                 }
                 case PRIM_TRIANGLE:
                 {
                     hit = HitTriangle(*scene.tris, indexInAoS, r, Interval(ray_t.min, closestSoFar), tempRec);
                     break;
                 }
                 default:
                 {
                     hit = false;
                     break;
                 }
                 }

                 if (hit)
                 {
                     hitAny = true;
                     closestSoFar = tempRec.t;
                     rec = tempRec;

                 }
             }
             continue;
         }
         else //if its not a leaf node, push children, near one last so it is popped first
         {
             const BVHNode& left = scene.BVHNodes[node.leftFirst];
             const BVHNode& right = scene.BVHNodes[node.rightFirst];

             //check if the ray hit any of the children.
             float tLeft = IntersectAABB(r, left.bboxMin, left.bboxMax, closestSoFar);
             float tRight = IntersectAABB(r, right.bboxMin, right.bboxMax, closestSoFar);
             bool hitLeft = tLeft >= 0.0f;
             bool hitRight = tRight >= 0.0f;

             //if both children are hit, put the further one first on the stack so near is processed first
             if (hitLeft && hitRight)
             {
                 if (tLeft < tRight)
                 {
                     if (stackPtr < MAX_STACK) stack[stackPtr++] = node.rightFirst; //far
                     if (stackPtr < MAX_STACK) stack[stackPtr++] = node.leftFirst;  //near
                 }
                 else
                 {
                     if (stackPtr < MAX_STACK) stack[stackPtr++] = node.leftFirst;  //near
                     if (stackPtr < MAX_STACK) stack[stackPtr++] = node.rightFirst; //far
                 }
             }
             //one children hit , jsut push that one on stack
             else if (hitLeft)
             {
                 if (stackPtr < MAX_STACK) stack[stackPtr++] = node.leftFirst;
             }
             else if (hitRight)
             {
                 if (stackPtr < MAX_STACK) stack[stackPtr++] = node.rightFirst;
             }
         }
         
     }

     return hitAny;

}