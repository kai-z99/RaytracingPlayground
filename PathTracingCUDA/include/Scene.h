#pragma once

#include "Material.h"
#include "Sphere.h"
#include "Quad.h"

struct Scene
{
    SpheresPacked* spheres;
    QuadsPacked* quads;

    MaterialData* materials;
    uint32_t materialCount;
};

 __device__ inline bool HitScene(const Scene& scene, const Ray& r, Interval ray_t, HitRecord& rec)
{
     bool hitAny = false;
     float closestSoFar = ray_t.max;
     HitRecord tempRec;

     if (scene.spheres)
     {
         for (int i = 0; i < scene.spheres->n; i++)
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
         for (int i = 0; i < scene.quads->n; i++)
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