#include "Hittable.h"

class Sphere : public Hittable
{
public:
    __device__ Sphere(const glm::dvec3& center, double radius, Material* mat) : center(center), radius(radius < 0.0 ? 0.0 : radius), mat(mat)
    {
        glm::dvec3 radiusVec = glm::dvec3(radius);
        this->bbox = AABB(center - radiusVec, center + radiusVec);
    }

    __device__ bool Hit(const Ray& r, Interval ray_t, HitRecord& rec) const override
	{
        glm::dvec3 oc = center - r.origin();
        double a = glm::dot(r.direction(), r.direction());
        double b = -2.0 * glm::dot(r.direction(), oc);
        double c = glm::dot(oc, oc) - radius * radius;
        double discriminant = (b * b) - (4 * a * c);
        if (discriminant < 0) //no solution
        {
            return false;
        }

        double sqrt = std::sqrt(discriminant);
        double root = (-b - sqrt) / (2.0 * a);
        
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
        rec.mat = mat;

        return true;
	}

    __device__ AABB BoundingBox() const override
    {
        return this->bbox;
    }

private:
	glm::dvec3 center;
	double radius;
    Material* mat;
    AABB bbox;
};