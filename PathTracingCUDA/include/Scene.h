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
     bool hit = false;

     for (int i = 0; i < scene.spheres->n; i++)
     {
         hit |= HitSphere(*scene.spheres, i, r, ray_t, rec);
     }

     return hit;

}