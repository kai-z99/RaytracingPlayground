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

class Dialectric : public Material
{
public:
	Dialectric(double refractionIndex) : refractionIndex(refractionIndex) {}

	bool Scatter(const Ray& rayIn,
		const HitRecord& rec,
		glm::dvec3& attenuation,
		Ray& scattered) const override
	{	
		//n1/n2 where n1 = refractive index of air = 1.0
		//ff: air into dialectric, 1/n2
		//bf: dialectric into air, n2/1
		bool frontFace = glm::dot(rayIn.direction(), rec.normal) < 0;
		glm::dvec3 N = frontFace ? rec.normal : -rec.normal;

		double eta = frontFace ? (1.0 / refractionIndex) : refractionIndex;
		double cosTheta = std::fmin(glm::dot(-rayIn.direction(), N), 1.0);
		double sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);

		//must reflect
		glm::dvec3 direction;
		if (eta * sinTheta > 1.0 || this->FresnelSchlick(cosTheta, refractionIndex) > RandomDouble())
		{
			direction = glm::reflect(rayIn.direction(), N);
		}
		else
		{
			direction = glm::refract(rayIn.direction(), N, eta);
		}
		
		
		scattered = Ray(rec.p, direction);
		attenuation = glm::dvec3(1.0);

		return true;
	}

private:
	double refractionIndex;

	//Returns kS
	static double FresnelSchlick(double cosTheta, double refractionIndex)
	{
		double r0 = (1 - refractionIndex) / (1 + refractionIndex);
		r0 = r0 * r0;
		return r0 + (1 - r0) * std::pow((1 - cosTheta), 5);
	}
};