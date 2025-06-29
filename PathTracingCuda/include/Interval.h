#pragma once

#include "Generic.h" //thank you pragma once
#include "CudaHelper.h"

class Interval
{
public:
	double min, max;

	__device__ Interval();
	__device__ Interval(double min, double max);
	__device__ Interval(const Interval& a, const Interval& b);

	__device__ double Size() const;
	__device__ bool Contains(double x) const;

	__device__ bool Surrounds(double x) const;

	__device__ double Clamp(double x) const;

	__device__ Interval Expand(double delta) const;

	static const Interval Empty, Universe;
};
