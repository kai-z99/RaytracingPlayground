#pragma once

#include "Generic.h"

class HitRecord
{
public:
	glm::vec3 p;
	glm::vec3 normal; //ray oriented
    glm::vec3 geoNormal; //mesh orianted
	float t;
	int matDataID;
};

struct HitList
{
    static constexpr int MAX_HITS = 128;
    HitRecord hits[MAX_HITS];
    int       count = 0;

    __device__ void reset() { count = 0; }

    __device__ bool add(const HitRecord& r)
    {
        if (count >= MAX_HITS) return false;
        hits[count++] = r;
        return true;
    }
};