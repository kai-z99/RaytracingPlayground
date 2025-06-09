#pragma once

#include "Hittable.h"

class Material
{
public:
	virtual ~Material() = default;

	virtual bool Scatter(
		const Ray& rayIn,
		const HitRecord& rec, 
		glm::dvec3& attenuation, //attenutation is the resulting rgb AFTER absorption
		Ray& scattered) const 
	{
		return false; 
	}
};

class Lambertian : public Material
{
public:
	Lambertian(const glm::dvec3& albedo) : albedo(albedo) {}

	bool Scatter(const Ray& rayIn,
		const HitRecord& rec,
		glm::dvec3& attenuation,
		Ray& scattered) const override
	{
		glm::dvec3 scatterDirection = rec.normal + glm::sphericalRand(1.0);

		if (NearZero(scatterDirection))
		{
			scatterDirection = rec.normal;
		}

		scattered = Ray(rec.p, scatterDirection);
		attenuation = albedo;
		return true;
	}

private:
	glm::dvec3 albedo;
};

class Metal : public Material
{
public:
	Metal(const glm::dvec3& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1? fuzz : 1.0) {}

	bool Scatter(const Ray& rayIn,
		const HitRecord& rec,
		glm::dvec3& attenuation,
		Ray& scattered) const override
	{
		glm::dvec3 reflectDirection = glm::reflect(rayIn.direction(), rec.normal);
		reflectDirection += fuzz * glm::sphericalRand(1.0);
		reflectDirection = glm::normalize(reflectDirection);
		scattered = Ray(rec.p, reflectDirection);
		attenuation = albedo;
		return glm::dot(scattered.direction(), rec.normal) > 0;
	}

private:
	glm::dvec3 albedo;
	double fuzz;
};