#pragma once

#include "../include/Interval.h"

__device__ Interval::Interval() : min(+infinity), max(-infinity) {}
__device__ Interval::Interval(double min, double max) : min(min), max(max) {}
__device__ Interval::Interval(const Interval& a, const Interval& b)
{
	this->min = (a.min <= b.min) ? a.min : b.min;
	this->max = (a.max >= b.max) ? a.max : b.max;
}

__device__ double Interval::Size() const
{
	return max - min;
}

__device__ bool Interval::Contains(double x) const
{
	return min <= x && x <= max;
}

__device__ bool Interval::Surrounds(double x) const
{
	return min < x && x < max;
}

__device__ double Interval::Clamp(double x) const
{
	if (x < min) return min;
	if (x > max) return max;
	return x;
}

__device__ Interval Interval::Expand(double delta) const
{
	double padding = delta / 2.0;
	return Interval(this->min - padding, this->max + padding);
}

const Interval Interval::Empty = Interval(+infinity, -infinity);
const Interval Interval::Universe = Interval(-infinity, +infinity);
