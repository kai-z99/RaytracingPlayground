#pragma once

#include "../include/Interval.h"

__device__ Interval::Interval() : min(+infinity), max(-infinity) {}
__device__ Interval::Interval(float min, float max) : min(min), max(max) {}
__device__ Interval::Interval(const Interval& a, const Interval& b)
{
	this->min = (a.min <= b.min) ? a.min : b.min;
	this->max = (a.max >= b.max) ? a.max : b.max;
}

__device__ float Interval::Size() const
{
	return max - min;
}

__device__ bool Interval::Contains(float x) const
{
	return min <= x && x <= max;
}

__device__ bool Interval::Surrounds(float x) const
{
	return min < x && x < max;
}

__device__ float Interval::Clamp(float x) const
{
	if (x < min) return min;
	if (x > max) return max;
	return x;
}

__device__ Interval Interval::Expand(float delta) const
{
	float padding = delta / 2.0f;
	return Interval(this->min - padding, this->max + padding);
}

const Interval Interval::Empty = Interval(+infinity, -infinity);
const Interval Interval::Universe = Interval(-infinity, +infinity);

