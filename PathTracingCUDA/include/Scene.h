#pragma once

#include "Material.h"
#include "Sphere.h"
#include "Quad.h"
#include "BVH.h"

//device only
struct Scene
{
    SpheresPacked* spheres;
    QuadsPacked* quads;

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
     bool hitAny = false;
     float closestSoFar = ray_t.max;
     HitRecord tempRec;

     if (scene.spheres)
     {
         for (uint32_t i = 0; i < scene.spheres->n; i++)
         {
             if (HitSphere(*scene.spheres, i, r, Interval(ray_t.min, closestSoFar), tempRec))
             {
                 hitAny = true;
                 closestSoFar = tempRec.t;
                 rec = tempRec;
             }
         }
     }

     if (scene.quads)
     {
         for (uint32_t i = 0; i < scene.quads->n; i++)
         {
             if (HitQuad(*scene.quads, i, r, Interval(ray_t.min, closestSoFar), tempRec))
             {
                 hitAny = true;
                 closestSoFar = tempRec.t;
                 rec = tempRec;
             }
         }
     }
     
     return hitAny;
}