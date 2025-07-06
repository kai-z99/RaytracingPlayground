#pragma once
#include "Hittable.h"

struct SpheresPacked
{
    glm::vec4* centerRadius;
    int* materialID;
    uint32_t n;
};

__device__ inline bool HitSphere(const SpheresPacked& s, int i, const Ray& r, Interval ray_t, HitRecord& rec)
{
    glm::vec3 center = glm::vec3(s.centerRadius[i]);
    float radius = s.centerRadius[i].w;

    glm::vec3 oc = center - r.origin();
    float a = glm::dot(r.direction(), r.direction());
    float b = -2.0f * glm::dot(r.direction(), oc);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = (b * b) - (4.0f * a * c);
    if (discriminant < 0.0f) //no solution
    {
        return false;
    }

    float sqrt = std::sqrt(discriminant);
    float root = (-b - sqrt) / (2.0f * a);

    if (!ray_t.Surrounds(root))
    {
        root = (-b + sqrt) / (2.0f * a);

        //can happen from refraction etc
        if (!ray_t.Surrounds(root))
        {
            return false;
        }
    }

    rec.t = root;
    rec.p = r.at(rec.t);
    rec.normal = (rec.p - center) / radius; //normalized
    rec.matDataID = s.materialID[i];

    return true;
}

