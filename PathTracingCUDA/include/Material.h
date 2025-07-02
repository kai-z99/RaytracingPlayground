#pragma once

#include "Hittable.h"

class Material
{
public:
	__device__ virtual ~Material() = default;

	__device__ virtual bool Scatter(
		curandState& randState,
		const Ray& rayIn,
		const HitRecord& rec, 
		glm::vec3& attenuation, //attenutation is the resulting rgb AFTER absorption
		Ray& scattered) const 
	{
		return false; 
	}
};

class Lambertian : public Material
{
public:
	__device__ Lambertian(const glm::vec3& albedo) : albedo(albedo) {}

	__device__ bool Scatter(
		curandState& randState,
		const Ray& rayIn,
		const HitRecord& rec,
		glm::vec3& attenuation,
		Ray& scattered) const override
	{
		glm::vec3 scatterDirection = rec.normal + RandomOnUnitSphere(randState); //FIX ME SPEHRICAL RAND

		if (NearZero(scatterDirection))
		{
			scatterDirection = rec.normal;
		}

		scattered = Ray(rec.p, scatterDirection);
		attenuation = albedo;
		return true;
	}

private:
	glm::vec3 albedo;
};

class Metal : public Material
{
public:
	__device__ Metal(const glm::vec3& albedo, float fuzz) : albedo(albedo), fuzz(fuzz < 1? fuzz : 1.0f) {}

	__device__ bool Scatter(
		curandState& randState,
		const Ray& rayIn,
		const HitRecord& rec,
		glm::vec3& attenuation,
		Ray& scattered) const override
	{
		glm::vec3 reflectDirection = glm::reflect(rayIn.direction(), rec.normal);
		reflectDirection += fuzz * RandomOnUnitSphere(randState); //FIX ME SPEHRICAL RAND
		reflectDirection = glm::normalize(reflectDirection);
		scattered = Ray(rec.p, reflectDirection);
		attenuation = albedo;
		return glm::dot(scattered.direction(), rec.normal) > 0.0f;
	}

private:
	glm::vec3 albedo;
	float fuzz;
};

class Dialectric : public Material
{
public:
	__device__ Dialectric(float refractionIndex) : refractionIndex(refractionIndex) {}

	__device__ bool Scatter(
		curandState& randState,
		const Ray& rayIn,
		const HitRecord& rec,
		glm::vec3& attenuation,
		Ray& scattered) const override
	{	
		//n1/n2 where n1 = refractive index of air = 1.0
		//ff: air into dialectric, 1/n2
		//bf: dialectric into air, n2/1
		bool frontFace = glm::dot(rayIn.direction(), rec.normal) < 0;
		glm::vec3 N = frontFace ? rec.normal : -rec.normal;

		float eta = frontFace ? (1.0 / refractionIndex) : refractionIndex;
		float cosTheta = std::fmin(glm::dot(-rayIn.direction(), N), 1.0f);
		float sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);

		//must reflect
		glm::vec3 direction;
		if (eta * sinTheta > 1.0 || this->FresnelSchlick(cosTheta, refractionIndex) > RandomFloat(randState)) //FIX ME
		{
			direction = glm::reflect(rayIn.direction(), N);
		}
		else
		{
			direction = glm::refract(rayIn.direction(), N, eta);
		}
		
		
		scattered = Ray(rec.p, direction);
		attenuation = glm::vec3(1.0);

		return true;
	}

private:
	float refractionIndex;

	//Returns kS
	__device__ static float FresnelSchlick(float cosTheta, float refractionIndex)
	{
		float r0 = (1 - refractionIndex) / (1 + refractionIndex);
		r0 = r0 * r0;
		return r0 + (1 - r0) * std::pow((1 - cosTheta), 5);
	}
};

