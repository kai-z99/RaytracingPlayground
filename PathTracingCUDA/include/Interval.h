#pragma once

#include "../include/Generic.h"

class Interval
{
public:
	float min, max;

	__device__ Interval();
	__device__ Interval(float min, float max);
	__device__ Interval(const Interval& a, const Interval& b);

	__device__ float Size() const;
	__device__ bool Contains(float x) const;

	__device__ bool Surrounds(float x) const;

	__device__ float Clamp(float x) const;

	__device__ Interval Expand(float delta) const;

	static const Interval Empty, Universe;
};


