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


//NEW REFACTOR 
//----------

enum MaterialType
{
	MAT_LAMBERTIAN = 0,
	MAT_METAL,
	MAT_DIALECTRIC,
};

struct MaterialData
{
	glm::vec3 color;
	float fuzz;
	float refractionIndex; //eta
	MaterialType type;
};

__device__ inline MaterialData* MakeLambertian(const glm::vec3& color)
{
	MaterialData* m = new MaterialData();
	m->color = color;
	m->fuzz = 0.0f;
	m->refractionIndex = 1.0f;
	m->type = MAT_LAMBERTIAN;
	return m;
}

__device__ inline MaterialData* MakeMetal(const glm::vec3& color, float fuzz)
{
	MaterialData* m = new MaterialData();
	m->color = color;
	m->fuzz = (fuzz < 1.0f ? fuzz : 1.0f);
	m->refractionIndex = 1.0f;
	m->type = MAT_METAL;
	return m;
}

__device__ inline MaterialData* MakeDialectric(float refractionIndex)
{
	MaterialData* m = new MaterialData();
	m->color = glm::vec3(1.0f);  
	m->fuzz = 0.0f;
	m->refractionIndex = refractionIndex;
	m->type = MAT_DIALECTRIC;
	return m;
}


__device__ inline float FresnelSchlick(float cosT, float eta)
{
	float r0 = (1 - eta) / (1 + eta);
	r0 *= r0;
	return r0 + (1 - r0) * powf(1 - cosT, 5);
}

__device__ inline bool Scatter(const MaterialData& materialData, 
						curandState& randState,
						const Ray& ray,
						const HitRecord& rec,
						glm::vec3& attenuation,
						Ray& scattered)
{
	switch (materialData.type)
	{
	case MAT_LAMBERTIAN:
	{
		glm::vec3 scatterDirection = rec.normal + RandomOnUnitSphere(randState); 

		if (NearZero(scatterDirection))
		{
			scatterDirection = rec.normal;
		}

		scattered = Ray(rec.p, scatterDirection);
		attenuation = materialData.color;
		return true;
	}

	case MAT_METAL:
	{
		glm::vec3 reflectDirection = glm::reflect(ray.direction(), rec.normal);
		reflectDirection += materialData.fuzz * RandomOnUnitSphere(randState); 
		reflectDirection = glm::normalize(reflectDirection);
		scattered = Ray(rec.p, reflectDirection);
		attenuation = materialData.color;
		return glm::dot(scattered.direction(), rec.normal) > 0.0f;
	}

	case MAT_DIALECTRIC:
	{
		//n1/n2 where n1 = refractive index of air = 1.0
		//ff: air into dialectric, 1/n2
		//bf: dialectric into air, n2/1
		bool frontFace = glm::dot(ray.direction(), rec.normal) < 0;
		glm::vec3 N = frontFace ? rec.normal : -rec.normal;

		float eta = frontFace ? (1.0 / materialData.refractionIndex) : materialData.refractionIndex;
		float cosTheta = std::fmin(glm::dot(-ray.direction(), N), 1.0f);
		float sinTheta = std::sqrt(1.0 - cosTheta * cosTheta);

		//must reflect
		glm::vec3 direction;
		if (eta * sinTheta > 1.0 || FresnelSchlick(cosTheta, materialData.refractionIndex) > RandomFloat(randState)) //FIX ME
		{
			direction = glm::reflect(ray.direction(), N);
		}
		else
		{
			direction = glm::refract(ray.direction(), N, eta);
		}


		scattered = Ray(rec.p, direction);
		attenuation = glm::vec3(1.0);

		return true;
	}

	default:
	{
		printf("UNEXPECTED MATERIAL\n");
		return false;
	}
		
	}
}