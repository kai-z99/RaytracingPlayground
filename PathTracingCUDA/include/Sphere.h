#pragma once
#include "Hittable.h"

class Sphere : public Hittable
{
public:
    __device__ Sphere(const glm::vec3& center, float radius, MaterialData* mat) : center(center), radius(radius < 0.0f ? 0.0f : radius), matData(mat)
    {
        glm::vec3 radiusVec = glm::vec3(radius);
        this->bbox = AABB(center - radiusVec, center + radiusVec);
    }

    __device__ bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const override
	{
        glm::vec3 oc = center - r.origin();
        float a = glm::dot(r.direction(), r.direction());
        float b = -2.0 * glm::dot(r.direction(), oc);
        float c = glm::dot(oc, oc) - radius * radius;
        float discriminant = (b * b) - (4 * a * c);
        if (discriminant < 0) //no solution
        {
            return false;
        }

        float sqrt = std::sqrt(discriminant);
        float root = (-b - sqrt) / (2.0 * a);
        
        if (!ray_t.Surrounds(root))
        {
            root = (-b + sqrt) / (2.0 * a);

            //can happen from refraction etc
            if (!ray_t.Surrounds(root))
            {
                return false;
            }
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        rec.normal = (rec.p - center) / radius; //normalized
        //rec.matData = matData;

        return true;
	}

    __device__ AABB BoundingBox() const override
    {
        return this->bbox;
    }

private:
	glm::vec3 center;
	float radius;
    MaterialData* matData;
    AABB bbox;
};

//REFACTOR
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
    float b = -2.0 * glm::dot(r.direction(), oc);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = (b * b) - (4 * a * c);
    if (discriminant < 0) //no solution
    {
        return false;
    }

    float sqrt = std::sqrt(discriminant);
    float root = (-b - sqrt) / (2.0 * a);

    if (!ray_t.Surrounds(root))
    {
        root = (-b + sqrt) / (2.0 * a);

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

