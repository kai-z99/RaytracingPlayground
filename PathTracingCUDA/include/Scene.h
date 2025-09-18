#pragma once

#include "Generic.h"
//#include "Material.h"
#include "Primitives.h"
#include "BVH.h"
#include "AABB.h"

//device only
struct MaterialData;
struct MaterialGPU;

struct EmissiveGeom 
{
    PrimType type;       // e.g., PRIM_SPHERE / PRIM_QUAD / PRIM_TRI
    uint32_t primIndex;  // index into spheres/quads/tris pack
    uint32_t matIndex;   // emissive material index
    float area;          // geometric area in world space
};

struct LightSetGPU 
{
    EmissiveGeom* lights;   // device array of emissive prim handles
    float* cdf;             // device array, non-decreasing [0..1], length==count
    uint32_t count;         // number of emissive prims
    float totalArea;        // sum of areas
};

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
    
    MaterialGPU* materials;
    uint32_t materialCount;

    //lighting
    LightSetGPU* lightSet;     // nullptr if no emissive prims
};

//traverse the bvh instead of linearly scanning
 __device__ inline bool HitScene(const Scene& scene, const Ray& r, Interval ray_t, HitRecord& rec, int* hitCount = nullptr)
{
     constexpr int MAX_STACK = 64;
     uint32_t stack[MAX_STACK]; //each element represents the index in BVHNodes*
     int stackPtr = 0;
     bool hitAny = false;
     float closestSoFar = ray_t.max;
     int count = 0;
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
                     count++;
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

     if (hitCount)
     {
         *hitCount = count;
     }
     return hitAny;

}


__device__ inline bool HitScene(const Scene& scene, const Ray& r, Interval ray_t, HitList& outHits)
{
    constexpr int MAX_STACK = 64;
    uint32_t      stack[MAX_STACK];
    int           stackPtr = 0;

    outHits.reset();

    // we don’t early–exit on the first or closest hit; we record them all
    stack[stackPtr++] = 0;
    while (stackPtr)
    {
        uint32_t nodeIdx = stack[--stackPtr];
        const BVHNode& node = scene.BVHNodes[nodeIdx];

        // if this node’s AABB is missed, skip it
        if (IntersectAABB(r, node.bboxMin, node.bboxMax, ray_t.max) < 0.0f)
            continue;

        // leaf?
        if (node.primCount)
        {
            for (uint32_t i = 0; i < node.primCount; ++i)
            {
                uint32_t idx = node.leftFirst + i;
                PrimType type = scene.primTypes[idx];
                uint32_t primIdx = scene.primIndices[idx];

                HitRecord tempRec;
                bool hit = false;
                switch (type)
                {
                case PRIM_SPHERE:
                    hit = HitSphere(*scene.spheres, primIdx, r, ray_t, tempRec); //only will record outer hit, careful
                    break;
                case PRIM_QUAD:
                    hit = HitQuad(*scene.quads, primIdx, r, ray_t, tempRec);
                    break;
                case PRIM_TRIANGLE:
                    hit = HitTriangle(*scene.tris, primIdx, r, ray_t, tempRec);
                    break;
                default:
                    break;
                }

                if (hit)
                {
                    // stash every hit
                    if (!outHits.add(tempRec)) return true;
                }
            }
        }
        else
        {
            // interior: push children if they might hit
            const BVHNode& L = scene.BVHNodes[node.leftFirst];
            const BVHNode& R = scene.BVHNodes[node.rightFirst];

            float tL = IntersectAABB(r, L.bboxMin, L.bboxMax, ray_t.max);
            float tR = IntersectAABB(r, R.bboxMin, R.bboxMax, ray_t.max);
            if (tL >= 0.0f) stack[stackPtr++] = node.leftFirst;
            if (tR >= 0.0f) stack[stackPtr++] = node.rightFirst;
        }
    }

    return outHits.count > 0;
}


__device__ inline bool HitSceneLinear(const Scene& scene, const Ray& r, Interval ray_t, HitRecord& rec)
{
    bool hitAny = false;
    float closestT = ray_t.max;
    HitRecord tmp;

    // --- Test every sphere ---
    for (uint32_t i = 0; i < scene.spheres->n; ++i) {
        if (HitSphere(*scene.spheres,
            i,
            r,
            Interval(ray_t.min, closestT),
            tmp))
        {
            hitAny = true;
            closestT = tmp.t;
            rec = tmp;
        }
    }

    // --- Test every quad ---
    for (uint32_t i = 0; i < scene.quads->n; ++i) {
        if (HitQuad(*scene.quads,
            i,
            r,
            Interval(ray_t.min, closestT),
            tmp))
        {
            hitAny = true;
            closestT = tmp.t;
            rec = tmp;
        }
    }

    // --- Test every triangle ---
    for (uint32_t i = 0; i < scene.tris->n; ++i) {
        if (HitTriangle(*scene.tris,
            i,
            r,
            Interval(ray_t.min, closestT),
            tmp))
        {
            hitAny = true;
            closestT = tmp.t;
            rec = tmp;
        }
    }

    return hitAny;
}