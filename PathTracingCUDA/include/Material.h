#pragma once

#include "Hittable.h"

//NEW REFACTOR 
//----------

enum MaterialType
{
	MAT_PBR,
	MAT_DIALECTRIC,
	MAT_LIGHT_DIFFUSE,
};

struct MaterialData
{
	glm::vec3 albedo;
	glm::vec3 emission;

	float metallic;
	float roughness;

	float refractionIndex; //eta
	MaterialType type;
};

__device__ inline float FresnelSchlick(float cosT, float eta)
{
	float r0 = (1 - eta) / (1 + eta);
	r0 *= r0;
	return r0 + (1 - r0) * powf(1 - cosT, 5);
}

//for precomputed F0
__device__ inline glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& F0)
{
	return F0 + (glm::vec3(1.0f) - F0) * powf(1.0f - cosTheta, 5.0f);
}


__device__ inline bool ScatterDielectric(const MaterialData& materialData,
	curandState& randState,
	const Ray& ray,
	const HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf);

__device__ inline bool ScatterGGX(const MaterialData& materialData,
	curandState& randState,
	const Ray& ray,
	const HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf);


__device__ inline bool ScatterLambertian(const MaterialData& materialData,
	curandState& randState,
	const Ray& ray,
	const HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf);



__device__ inline bool Scatter(const MaterialData& materialData, 
						curandState& randState,
						const Ray& ray,
						const HitRecord& rec,
						glm::vec3& attenuation,
						Ray& scattered,
						float& pdf)
{
	//light
	if (materialData.type == MAT_LIGHT_DIFFUSE) return false;

	//dielectric 
	if (materialData.type == MAT_DIALECTRIC) return ScatterDielectric(materialData, randState, ray, rec, attenuation, scattered, pdf);


	float r = RandomFloat(randState);

	if (r < materialData.metallic)
	{
		return ScatterGGX(materialData, randState, ray, rec, attenuation, scattered, pdf);
	}
	else
	{
		return ScatterLambertian(materialData, randState, ray, rec, attenuation, scattered, pdf);
	}
}


__device__ inline bool ScatterDielectric(const MaterialData& materialData,
	curandState& randState,
	const Ray& ray,
	const HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf)
{
	//n1/n2 where n1 = refractive index of air = 1.0
	//ff: air into dialectric, 1/n2
	//bf: dialectric into air, n2/1
	bool frontFace = glm::dot(ray.direction(), rec.normal) < 0.0f;
	glm::vec3 N = frontFace ? rec.normal : -rec.normal;

	float eta = frontFace ? (1.0f / materialData.refractionIndex) : materialData.refractionIndex;
	float cosTheta = std::fmin(glm::dot(-ray.direction(), N), 1.0f);
	float sinTheta = std::sqrtf(1.0f - cosTheta * cosTheta);

	float F = FresnelSchlick(cosTheta, materialData.refractionIndex);

	//must reflect
	glm::vec3 direction;
	if (eta * sinTheta > 1.0 || F > RandomFloat(randState))
	{
		direction = glm::reflect(ray.direction(), N);
		pdf = F;
		attenuation = glm::vec3((F / pdf));
	}
	else
	{
		direction = glm::refract(ray.direction(), N, eta);
		pdf = 1.0f - F;
		attenuation = glm::vec3((1.0f - F) / pdf); //* ((1.0f - F) / pdf) = pdf/pdf = 1
	}

	scattered = Ray(rec.p, direction);
	
	return true;
}

//Tangent -> World
__device__ inline void BuildTBN(glm::vec3& T, glm::vec3& B, const glm::vec3 N)
{
	if (fabsf(N.x) > fabsf(N.z))
	{
		T = glm::normalize(glm::vec3(-N.y, N.x, 0.0f));
	}
	else
	{
		T = glm::normalize(glm::vec3(0.0f, -N.z, N.y));
	}

	B = glm::cross(N, T);
}

__device__ inline float D_GGX(float NdotH, float alpha)
{
	float a2 = alpha * alpha;
	float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
	return a2 / (pi * denom * denom);
}

__device__ inline float G1_SchlickGGX(float NdotX, float k)
{
	return NdotX / (NdotX * (1.0f - k) + k);
}

__device__ inline float G_Smith(float NdotV, float NdotL, float roughness)
{
	float k = roughness + 1.0f;
	k = (k * k) / 8.0f;
	float gv = G1_SchlickGGX(NdotV, k);
	float gl = G1_SchlickGGX(NdotL, k);

	return gv * gl;
}

__device__ inline glm::vec3 SampleGGX(const glm::vec3& N, float roughness, curandState& randState, float& pdf)
{
	//cos(theta) = sqrt((1 - zeta1) / (zeta1(a^2 - 1) + 1) )
	//phi = 2pi * zeta2

	float zeta1 = RandomFloat(randState);
	float zeta2 = RandomFloat(randState);

	float a = roughness * roughness;

	float phi = 2.0f * pi * zeta2;

	float cosTheta = sqrtf((1.0f - zeta1) / (1.0f + zeta1 * (a * a - 1.0f)));
	float sinTheta = sqrtf(1.0f - cosTheta * cosTheta);

	glm::vec3 hTan = glm::vec3(cosf(phi) * sinTheta, sinf(phi) * sinTheta, cosTheta);

	glm::vec3 T, B;
	BuildTBN(T, B, N);
	glm::vec3 halfway = glm::normalize(glm::vec3(hTan.x * T + hTan.y * B + hTan.z * N));

	float NdotH = fmaxf(glm::dot(N, halfway), 0.0f); //costheta
	float D = D_GGX(NdotH, a);
	pdf = D * NdotH;

	return halfway;
}


__device__ inline bool ScatterGGX(const MaterialData& materialData,
	curandState& randState,
	const Ray& ray,
	const HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf)
{
	
	glm::vec3 N = rec.normal;
	glm::vec3 V = glm::normalize(-ray.direction());

	if (materialData.roughness < 1e-6)
	{
		glm::vec3 L = glm::reflect(-V, N);
		scattered = Ray(rec.p, L);
		attenuation = glm::vec3(1.0);   // mirror BRDF = delta
		pdf = 1.0f;
		return true;
	}

	float pdfHalf;
	glm::vec3 halfway = SampleGGX(N, materialData.roughness, randState, pdfHalf);

	//reflect on the haldway vector to get sample vector
	glm::vec3 L = glm::reflect(-V, halfway);
	if (glm::dot(L, N) <= 0.0f) return false;

	pdf = pdfHalf / (4.0f * fabsf(dot(L, halfway)));
	if (pdf < 1e-6f) pdf = 1e-6f;

	//evalyate BRDF to find attenuation
	float NdotL = fmaxf(glm::dot(N, L), 0.0f);
	float NdotV = fmaxf(glm::dot(N, V), 0.0f);
	float NdotH = fmaxf(glm::dot(N, halfway), 0.0f);

	float D = D_GGX(NdotH, materialData.roughness * materialData.roughness);
	float G = G_Smith(NdotV, NdotL, materialData.roughness);

	glm::vec3 F0 = mix(glm::vec3(0.04f), materialData.albedo, materialData.metallic);
	float VdotH = glm::clamp(glm::dot(V, halfway), 0.0f, 1.0f);
	glm::vec3 F = FresnelSchlick(VdotH, F0);

	glm::vec3 specular = (D * G * F) / (4.0f * NdotV * NdotL + 1e-6f);
	attenuation = specular * (NdotL / pdf); //costheta * brdf / pdf
	scattered = Ray(rec.p, L);

	return true;
}

__device__ inline glm::vec3 SampleLambertian(const glm::vec3& N, curandState& randState, float& pdf)
{
	//cos(theta) = sqrt(1 - zeta1)
	//phi = 2pi * zeta2


	float zeta1 = RandomFloat(randState);
	float zeta2 = RandomFloat(randState);

	float r = sqrtf(zeta1); //sin theta
	float phi = 2.0f * pi * zeta2;
	float x = r * cosf(phi);
	float y = r * sinf(phi);
	float z = sqrtf(1.0f - zeta1); //cos theta

	glm::vec3 T, B;
	BuildTBN(T, B, N);

	glm::vec3 L = (x * T) + (y * B) + (z * N);

	pdf = z / pi;

	return glm::normalize(L);

}

__device__ inline bool ScatterLambertian(
	const MaterialData& materialData,
	curandState& randState,
	const Ray& ray,
	const HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf)
{
	glm::vec3 L = SampleLambertian(rec.normal, randState, pdf);
	if (pdf < 1e-6f) return false;
	scattered = Ray(rec.p, L);
	attenuation = materialData.albedo; //albedo / pi = ...
	return true;
}


