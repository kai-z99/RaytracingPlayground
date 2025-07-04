#pragma once

#include "Material.h"
#include "Sphere.h"

struct Scene
{
    MaterialData* materials;
    uint32_t materialCount;
    SpheresPacked* spheres;
    //other packed
};

 __device__ inline bool HitScene(const Scene& scene, const Ray& r, Interval ray_t, HitRecord& rec)
{
     bool hitAny = false;
     float closestSoFar = ray_t.max;
     HitRecord tempRec;

     for (int i = 0; i < scene.spheres->n; i++)
     {
         if (HitSphere(*scene.spheres, i, r, Interval(ray_t.min, closestSoFar), tempRec))
         {
             hitAny = true;
             closestSoFar = tempRec.t;
             rec = tempRec;
         }
     }

     return hitAny;

}