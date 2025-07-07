#pragma once

#include "Generic.h"

class HitRecord
{
public:
	glm::vec3 p;
	glm::vec3 normal;
	float t;
	int matDataID;
};
