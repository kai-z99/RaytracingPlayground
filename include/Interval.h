#pragma once

#include "Generic.h" //thank you pragma once

class Interval
{
public:
	double min, max;

	Interval() : min(+infinity), max(-infinity) {}
	Interval(double min, double max) : min(min), max(max) {}
	Interval(const Interval& a, const Interval& b)
	{
		this->min = (a.min <= b.min) ? a.min : b.min;
		this->max = (a.max >= b.max) ? a.max : b.max;
	}

	double Size() const
	{
		return max - min;
	}

	bool Contains(double x) const 
	{
		return min <= x && x <= max;
	}

	bool Surrounds(double x) const 
	{
		return min < x && x < max;
	}

	double Clamp(double x) const
	{
		if (x < min) return min;
		if (x > max) return max;
		return x;
	}

	Interval Expand(double delta) const
	{
		double padding = delta / 2.0;
		return Interval(this->min - padding, this->max + padding);
	}

	static const Interval Empty, Universe;
};

const Interval Interval::Empty = Interval(+infinity, -infinity);
const Interval Interval::Universe = Interval(-infinity, +infinity);
