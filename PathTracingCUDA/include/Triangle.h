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
	glm::vec3 edge1 = tris.p1[i]; - p0;
	glm::vec3 edge2 = tris.p2[i] - p0;
	
	/*
	Any point in the triangle can be written as:

		P = p0 + u(p1−p0) + v(p2−p0) for some u, v.

	Note that u >= 0 , v >= 0, u+v <= 1.
	 
	So an intersection occurs when we can find some t, u, v for r(t) = P
	The question turns into:

		O + tD = p0 + u(p1−p0) + v(p2−p0)
		tD - u*e1 - v*e2 = p0 - O

	That is: 
	
		Ax = B 
	
	where x = [t,u,v], A = {D, -e1, -e2}, B = [p0 - O].

	You can solve this Cramer's rule:

		t = det(A1)/det(A)
		u = det(A2)/det(A)
		v = det(A3)/det(A)
		.. Where for A_i you replace column i with B

	Since det([a,b,c]) = dot(a, cross(b,c)):

		det(A) = dot(D, cross(-e1, -e2)) = dot(D, cross(e1, e2))

	So:
		
		det(A1) = det([B, -e1, -e2]) = dot((p0 - O), cross(e1,e2))
		det(A2) =					 = -dot(D, cross((p0 - O), e2))
		det(A3) =					 = -dot(D, cross((e1, (p0 - O))
		

	Therefore:

		t = dot((p0 - O), cross(e1,e2)) / dot(D, cross(e1, e2))
		u = dot(D, cross((O - p0), e2)) / dot(D, cross(e1, e2))
		v = dot(D, cross((e1, (O - p0)) / dot(D, cross(e1, e2))
	
	(Note that we take -B = O - p0 to absorb the negative sign in front of e1 and e2)
	*/


	glm::vec3 rayCrossE2 = glm::cross(r.direction(), edge2);

	// this is actually -det(A).
	float det = glm::dot(edge1, rayCrossE2); 

	if (fabs(det) < 1e-8f)
	{
		return false; //the ray is parallel to triangle
	}

	float invDet = 1.0f / det;
	glm::vec3 s = r.origin() - p0; //-B

	//"u = dot(D, cross((O - p0), e2)) / dot(D, cross(e1, e2))" 
	// Note: The numerator in our calculation is -Det(A2). But since we are also using -Det(A) in the denom, it cancels
	float u = invDet * glm::dot(s, rayCrossE2);

	//"Note that u >= 0 , v >= 0, u+v <= 1."
	if ((u < 0 && fabs(u) > 1e-8) || (u > 1 && fabs(u - 1) > 1e-8))
	{
		//printf("FALSE2");
		return false;
	}

	glm::vec3 sCrossE1 = glm::cross(s, edge1);

	//"v = dot(D, cross((e1, (O - p0)) / dot(D, cross(e1, e2))"
	// Note: The numerator in our calculation is -Det(A3). But since we are also using -Det(A) in the denom, it cancels
	float v = invDet * glm::dot(r.direction(), sCrossE1);


	//" Note that v >= 0, u + v <= 1."
	if ((v < 0 && fabs(v) > 1e-8) || (u + v > 1 && fabs(u + v - 1) > 1e-8))
	{
		return false;
	}

	//"t = dot((p0 - O), cross(e1,e2)) / dot(D, cross(e1, e2))"
	float t = invDet * glm::dot(edge2, sCrossE1);

	if (!ray_t.Contains(t))
	{
		return false;
	}
	else
	{
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