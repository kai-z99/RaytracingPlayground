#pragma once

#include "Generic.h"
#include "Hittable.h"

struct QuadsPacked
{
	glm::vec3* Q;
	glm::vec3* u;
	glm::vec3* v;
	int* materialID;
	uint32_t n;
};

__device__ inline bool HitQuad(const QuadsPacked& quads, int i, const Ray& r, Interval ray_t, HitRecord& rec)
{
	glm::vec3 q = quads.Q[i];
	glm::vec3 u = quads.u[i];
	glm::vec3 v = quads.v[i];

	glm::vec3 n = glm::cross(u,v);
	float d = dot(n, q); //ax + by + cz = d

	float denom = glm::dot(n, r.direction());
	if (fabs(denom) < 1e-8) return false; //ray is parralell to plane

	float t = (d - glm::dot(n, r.origin())) / denom;
	if (!ray_t.Contains(t)) return false;

	glm::vec3 intersection = r.at(t); //intersection on plane

	//now check if its in the quad
	glm::vec3 p = intersection - q;
	glm::vec3 w = n / dot(n, n);

	float alpha = glm::dot(w, glm::cross(p, v));
	float beta = glm::dot(w, glm::cross(u, p));

	if ((-1e-8 <= alpha && alpha <= 1 + 1e-8) && (-1e-8 <= beta && beta <= 1 + 1e-8))
	{
		rec.t = t;
		rec.p = intersection;
		n = glm::normalize(n);
		rec.normal = (denom < 0.0f)? n : -n;
		rec.matDataID = quads.materialID[i];
		return true;
	}

	return false;
}


