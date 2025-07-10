#pragma once

#include "Generic.h"
#include "Hittable.h"

struct TrianglesPacked
{
	glm::vec3* p0;
	glm::vec3* p1;
	glm::vec3* p2;
	int* materialID;
	uint32_t n;
};

//moller trombore
__device__ inline bool HitTriangle(const TrianglesPacked& tris, int i, const Ray& r, Interval ray_t, HitRecord& rec)
{
	//printf("Test\n");
	glm::vec3 p0 = tris.p0[i];
	glm::vec3 p1 = tris.p1[i];
	glm::vec3 p2 = tris.p2[i];
	glm::vec3 edge1 = p1 - p0;
	glm::vec3 edge2 = p2 - p0;

	glm::vec3 rayCrossE2 = glm::cross(r.direction(), edge2);
	float det = glm::dot(edge1, rayCrossE2);

	if (fabs(det) < 1e-8f)
	{
		//printf("FALSE1");
		return false; //the ray is parallel to triangle
	}

	float invDet = 1.0f / det;
	glm::vec3 s = r.origin() - p0;
	float u = invDet * glm::dot(s, rayCrossE2);

	if ((u < 0 && fabs(u) > 1e-8) || (u > 1 && fabs(u - 1) > 1e-8))
	{
		//printf("FALSE2");
		return false;
	}

	glm::vec3 sCrossE1 = glm::cross(s, edge1);
	float v = invDet * glm::dot(r.direction(), sCrossE1);

	if ((v < 0 && fabs(v) > 1e-8) || (u + v > 1 && fabs(u + v - 1) > 1e-8))
	{
		//printf("FALSE3");
		return false;
	}

	float t = invDet * glm::dot(edge2, sCrossE1);

	if (!ray_t.Contains(t))
	{
		//printf("FALSE4");
		return false;
	}
	else
	{
		//printf("TRUE");
		glm::vec3 n = glm::normalize(glm::cross(edge1, edge2));
		rec.normal = (glm::dot(n, r.direction()) < 0.0f) ? n : -n;
		rec.t = t;
		rec.p = r.at(rec.t);
		rec.matDataID = tris.materialID[i];

		//rec.u = u;
		//rec.v = v;

		return true;
	}


}