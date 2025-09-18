#pragma once

#include "Sphere.h"
#include "Quad.h"
#include "Triangle.h"

enum PrimType
{
	PRIM_SPHERE = 0,
	PRIM_QUAD = 1,
	PRIM_TRIANGLE = 2,
};

__device__ inline void FetchSphere(const SpheresPacked* sp, uint32_t i,
    glm::vec3& c, float& r, int& matID)
{
    const glm::vec4 cr = sp->centerRadius[i]; // (cx,cy,cz,r)
    c = glm::vec3(cr);
    r = cr.w;
    matID = sp->materialID[i];
}

__device__ inline void FetchTriangle(const TrianglesPacked* tp, uint32_t i,
    glm::vec3& p0, glm::vec3& p1, glm::vec3& p2, int& matID)
{
    p0 = tp->p0[i];
    p1 = tp->p1[i];
    p2 = tp->p2[i];
    matID = tp->materialID[i];
}

__device__ inline void FetchQuad(const QuadsPacked* qp, uint32_t i,
    glm::vec3& Q, glm::vec3& u, glm::vec3& v, int& matID)
{
    Q = qp->Q[i];
    u = qp->u[i];
    v = qp->v[i];
    matID = qp->materialID[i];
}
