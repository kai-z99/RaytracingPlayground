#pragma once

#include "Generic.h"
#include "Interval.h"

__device__ inline float IntersectAABB(const Ray& r, const glm::vec3& bmin, const glm::vec3& bmax, float tMax, float tMin = 0.0f)
{
	const glm::vec3& rayOrigin = r.origin();
	const glm::vec3& rayDirection = r.direction();

	//[t0,t1] is the overall interval of all 3 axis's solutions
	float t0 = tMin;
	float t1 = tMax;

	#pragma unroll
	for (int axis = 0; axis < 3; axis++)
	{
		//solve t0 = (x_0 - Q_x) / d_x (for x,y,z)
		float invD = 1.0f / rayDirection[axis];
		float tNear = (bmin[axis] - rayOrigin[axis]) * invD;
		float tFar = (bmax[axis] - rayOrigin[axis]) * invD;
		if (invD < 0.0f)
		{
			float temp = tNear;
			tNear = tFar;
			tFar = temp;
		}

		//tighten the overall interval [t0,t1] based on the calculated ones for this axis.
		t0 = (tNear > t0) ? tNear : t0; //if the new near greater than the current, tighten.
		t1 = (tFar < t1) ? tFar : t1;   //if the new far is les than the current, tighten.

		//if the interval is empty, we did not hit the AABB.
		if (t1 < t0) return -1.0f;

		if (fabs(rayDirection[axis]) < 1e-8f) 
		{
			// Ray is parallel.  If origin is outside the slab, no hit.
			if (rayOrigin[axis] < bmin[axis] || rayOrigin[axis] > bmax[axis])
				return -1.0f;
			// otherwise continue with other slabs
			continue;
		}
	}

	return t0;
}

