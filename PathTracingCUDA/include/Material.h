#pragma once

#include "Hittable.h"
#include "Scene.h"
#include "CudaHelper.h"

extern __managed__ unsigned int noIntersection;
extern __managed__ unsigned int rejected;
extern __managed__ unsigned int total;
extern __managed__ unsigned int clampedPDFs;
extern __managed__ unsigned int PDFs;
extern __managed__ float radialSamplesSum;
extern __managed__ float radialSamplesCount;
extern __managed__ float expectedRadialAverage;
extern __managed__ float uSum;
extern __managed__ double sssEnergySumR;
extern __managed__ double sssEnergySumG;
extern __managed__ double sssEnergySumB;
extern __managed__ unsigned long long sssHitCount;
//
//----------
// 

enum MaterialType
{
	MAT_PBR = 0,
	MAT_DIALECTRIC,
	MAT_LIGHT_DIFFUSE,
	MAT_SUBSURFACE,
};

struct MaterialData
{
	MaterialType type;

	//base
	glm::vec3 albedo;
	glm::vec3 emission;

	//metallic-roughness
	float metallic;
	float roughness;

	//dielectric/sss
	float refractionIndex; 

	//burley's sss
	float subsurface;
	float sssRadius;
	glm::vec3 sssTint;

	//dipole sss
	float sigmaS;
	float sigmaA;

};


//NEW


enum MaterialTag
{
	LAMBERT,
	MICROFACET,
	DIELECTRIC,
	SUBSURFACE,
	EMISSIVE,
};

struct LambertParams
{
	glm::vec3 albedo;
};

struct MicrofacetParams
{
	glm::vec3 albedo;
	float metallic;
	float roughness;
};

struct DielectricParams
{
	float eta;
};

struct SubsurfaceParams
{
	glm::vec3 albedo;
	float ell;
	float eta;
};

struct EmissiveParams
{
	glm::vec3 emission;
};

struct MaterialGPU
{
	MaterialTag tag;

	union
	{
		LambertParams lambert;
		MicrofacetParams microfacet;
		DielectricParams dielectric;
		SubsurfaceParams subsurface;
		EmissiveParams emissive;
	};
};

struct BSDFSample
{
	glm::vec3 f;
	glm::vec3 wi;
	float pdf;
	bool isTransmission;
	bool good;
};

struct BSSRDFSample
{
	glm::vec3 f;
	glm::vec3 pi;
	glm::vec3 ni;
	float pdf;

	MaterialGPU exitBSDF;
	bool good;
};

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

__device__ inline BSDFSample SampleLambertBSDF(const LambertParams& params, const glm::vec3& wo, const glm::vec3& no, curandState& RNG)
{
	BSDFSample sample;

	//bxdf -> bsdf local frame
	glm::vec3 T, B;
	BuildTBN(T, B, no);

	float zeta1 = RandomFloat(RNG);
	float zeta2 = RandomFloat(RNG);

	float r = sqrtf(zeta1);
	float phi = 2.0f * pi * zeta2;
	float x = r * cosf(phi);
	float y = r * sinf(phi);
	float z = sqrtf(1.0f - zeta1); //cos theta

	//wi
	glm::vec3 L = glm::normalize((x * T) + (y * B) + (z * no));
	sample.wi = L;

	//pdf
	sample.pdf = z / pi;
	if (sample.pdf < 1e-6f)
	{
		sample.good = false;
		return sample;
	}
		
	//f
	sample.f = params.albedo / pi;

	//good
	sample.good = true;
	sample.isTransmission = false;
	return sample;

}

__device__ inline glm::vec3 SampleGGX_VNDF(const glm::vec3& N, const glm::vec3& V, float roughness, curandState& randState, float& pdfHalf);
__device__ inline glm::vec3 SampleGGX(const glm::vec3& N, float roughness, curandState& randState, float& pdfHalf);
__device__ inline float D_GGX(float NdotH, float alpha);
__device__ inline float G_SmithHeightCorrelated(float NdotV, float NdotL, float alpha);
__device__ inline float FresnelSchlick(float cosT, float eta);
__device__ inline glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& F0);
__device__ inline BSDFSample SampleMicrofacetBSDF(const MicrofacetParams& params, const glm::vec3& wo, const glm::vec3& no, curandState& RNG)
{
	BSDFSample sample;
	glm::vec3 N = no;
	glm::vec3 V = glm::normalize(wo);

	if (params.roughness < 0.04f) //fall back to mirror
	{
		glm::vec3 L = glm::reflect(-V, N);
		sample.wi = L;
		sample.f = glm::vec3(1.0);
		sample.pdf = 1.0f;
		return sample;
	}


	float pdfHalf;
	glm::vec3 halfway;

	//halfway = SampleGGX_VNDF(N, V, params.roughness, RNG, pdfHalf);
	halfway = SampleGGX(N, params.roughness, RNG, pdfHalf);

	//reflect on the haldway vector to get sample vector
	glm::vec3 L = glm::reflect(-V, halfway);

	if (glm::dot(L, N) <= 0.0f) 
	{
		sample.good = false;
		return sample;
	};

	//while (glm::dot(L, N) <= 0.0f)
	//{
	//	halfway = SampleGGX(N, materialData.roughness, randState, pdfHalf);
	//	L = glm::reflect(-V, halfway);
	//}
	//if (glm::dot(L, N) <= 0.0f) return false; //ENERGY LOSS WARNING

	sample.pdf = pdfHalf / (4.0f * fabsf(dot(V, halfway))); //changes of variables adds jacobian factor to pdf
	if (sample.pdf < 1e-6f)
	{
		sample.good = false;
		return sample;
	}

	//glm::vec3 L = SampleLambertian(N, randState, pdf); 
	//if (pdf < 1e-6f || glm::dot(L, N) <= 0.0f)           
	//	return false;
	//glm::vec3 halfway = glm::normalize(V + L);

	//evalyate BRDF to find attenuation
	float NdotL = fmaxf(glm::dot(N, L), 0.0f);
	float NdotV = fmaxf(glm::dot(N, V), 0.0f);
	float NdotH = fmaxf(glm::dot(N, halfway), 0.0f);

	float D = D_GGX(NdotH, params.roughness * params.roughness);
	float G = G_SmithHeightCorrelated(NdotV, NdotL, params.roughness * params.roughness);

	//TEMP
	glm::vec3 F0 = mix(glm::vec3(0.04f), params.albedo, params.metallic); //still hack
	float VdotH = glm::clamp(glm::dot(V, halfway), 0.0f, 1.0f);
	glm::vec3 F = FresnelSchlick(VdotH, F0);

	glm::vec3 specular = (D * G * F) / (4.0f * NdotV * NdotL + 1e-6f);
	sample.f = specular; //just the brdf
	sample.wi = L;
	sample.good = true;
	sample.isTransmission = false;

	return sample;
}

__device__ inline BSDFSample SampleDielectricBSDF(const DielectricParams& params, const glm::vec3& wo, const glm::vec3& no, curandState& RNG)
{
	return BSDFSample();
}

__device__ inline BSDFSample SampleSubsurfaceBSDF(const SubsurfaceParams& params, const glm::vec3& wo, const glm::vec3& no, curandState& RNG)
{
	DielectricParams p;
	p.eta = params.eta;
	return SampleDielectricBSDF(p, wo, no, RNG);
}

__device__ inline BSDFSample SampleEmissiveBSDF(const EmissiveParams& params, const glm::vec3& wo, const glm::vec3& no, curandState& RNG)
{
	BSDFSample s;
	s.good = false;
	return s;
}


__device__ inline BSDFSample ConstructAndSampleBSDF(const MaterialGPU& m, const glm::vec3& wo, const glm::vec3& no, curandState& RNG)
{
	switch (m.tag)
	{
	case LAMBERT:	 return SampleLambertBSDF	(m.lambert, wo, no, RNG);
	case MICROFACET: return SampleMicrofacetBSDF(m.microfacet, wo, no, RNG);
	case DIELECTRIC: return SampleDielectricBSDF(m.dielectric, wo, no, RNG);
	case SUBSURFACE: return SampleSubsurfaceBSDF(m.subsurface, wo, no, RNG);
	case EMISSIVE:   return SampleEmissiveBSDF(m.emissive, wo, no, RNG);
	}
}

__device__ inline BSSRDFSample SampleSubsurfaceBSSRDF(const SubsurfaceParams& params, const glm::vec3& po, const glm::vec3& no, curandState& RNG)
{
	return BSSRDFSample();
}

__device__ inline BSSRDFSample ConstructAndSampleBSSRDF(const MaterialGPU& m, const glm::vec3& po, const glm::vec3& no, curandState& RNG)
{
	if (m.tag != SUBSURFACE) return BSSRDFSample();

	return SampleSubsurfaceBSSRDF(m.subsurface, po, no, RNG);
}

__device__ inline float FrDielectricExact(float cosThetaI, float etaI, float etaT)
{
	cosThetaI = fmaxf(fminf(cosThetaI, 1.0f), -1.0f);
	bool entering = cosThetaI > 0.0f;
	if (!entering) { float t = etaI; etaI = etaT; etaT = t; cosThetaI = fabsf(cosThetaI); }
	float sin2I = fmaxf(0.f, 1.f - cosThetaI * cosThetaI);
	float eta = etaI / etaT, sin2T = eta * eta * sin2I;
	if (sin2T >= 1.f) return 1.f;
	float cosT = sqrtf(fmaxf(0.f, 1.f - sin2T));
	float rPar = ((etaT * cosThetaI) - (etaI * cosT)) / ((etaT * cosThetaI) + (etaI * cosT));
	float rPer = ((etaI * cosThetaI) - (etaT * cosT)) / ((etaI * cosThetaI) + (etaT * cosT));
	return 0.5f * (rPar * rPar + rPer * rPer);
}


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

__device__ bool SampleSubsurfaceDisk(const MaterialData& materialData,
	curandState& randState,
	const Scene& scene,
	const HitRecord& rec,
	glm::vec3& xi, //returned entry point
	glm::vec3& xiN, //return entry point normal
	float& pdfS,
	int& channel);

__device__ bool ScatterSubsurface(const MaterialData& materialData,
	curandState& randState,
	const Scene& scene,
	const Ray& ray,
	HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf);

__device__ inline bool Scatter(const MaterialData& materialData, 
						curandState& randState,
						const Scene& scene,
						const Ray& ray,
						HitRecord& rec,
						glm::vec3& attenuation,
						Ray& scattered,
						float& pdf,
						bool& prevSSS)
{
	//light
	if (materialData.type == MAT_LIGHT_DIFFUSE) return false;

	//dielectric 
	if (materialData.type == MAT_DIALECTRIC) return ScatterDielectric(materialData, randState, ray, rec, attenuation, scattered, pdf);

	float r = RandomFloat(randState);

	//SSS
	float wSSS = materialData.subsurface;
	if (materialData.type == MAT_SUBSURFACE && RandomFloat(randState) < wSSS)
	{
		//build first fresnel term
		float VdotN = fmaxf(glm::dot(-ray.direction(), rec.normal), 0.0f);
		float F_o = FrDielectricExact(VdotN, 1.0f, materialData.refractionIndex); //(1 - Fo) term. for light leaving surface

		//did light enter
		if (r < (1.0f - F_o))
		{
			if (prevSSS)
			{
				bool ok = ScatterLambertian(materialData, randState, ray, rec, attenuation, scattered, pdf);
				if (ok) prevSSS = false;
				return ok;
			}
			else
			{
				bool ok = ScatterSubsurface(materialData, randState, scene, ray, rec, attenuation, scattered, pdf);
				if (ok) prevSSS = true;
				return ok;
			}
			
		}
		else
		{
			prevSSS = false;
			bool ok = ScatterGGX(materialData, randState, ray, rec, attenuation, scattered, pdf);
			return ok;
		}
		//gofall though
	}

	prevSSS = false;

	r = RandomFloat(randState);
	//metallic-roughness
	{
		float wSpec = glm::clamp(materialData.metallic, 1e-6f, 1.0f - 1e-6f);
		float wDiff = 1.0f - wSpec;
		if (r < wSpec)
		{
			bool ok = ScatterGGX(materialData, randState, ray, rec, attenuation, scattered, pdf);
			return ok;
		}
		else
		{
			bool ok = ScatterLambertian(materialData, randState, ray, rec, attenuation, scattered, pdf);
			return ok;
		}
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
		direction = glm::refract(ray.direction(), N, eta); //specular transmission
		pdf = 1.0f - F;
		attenuation = glm::vec3((1.0f - F) / pdf); //* ((1.0f - F) / pdf) = pdf/pdf = 1
	}

	scattered = Ray(rec.p, direction);
	
	return true;
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

//isotropic GGX
__device__ inline float LambdaGGX(float cosTheta, float alpha) 
{
	float a2 = alpha * alpha;
	float cos2 = cosTheta * cosTheta;
	return (-1.0f + sqrtf(1.0f + a2 * (1.0f - cos2) / cos2)) * 0.5f;
}

__device__ inline float G_SmithHeightCorrelated(float NdotV, float NdotL, float alpha)
{
	return 1.0f / (1.0f + LambdaGGX(NdotV, alpha) + LambdaGGX(NdotL, alpha));
}


__device__ inline glm::vec3 SampleGGX(const glm::vec3& N, float roughness, curandState& randState, float& pdfHalf)
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
	pdfHalf = D * NdotH;

	return halfway;
}

// https://jcgt.org/published/0007/04/01/paper.pdf
// VNDF described by Eric Heitz (2018)
__device__ inline glm::vec3 SampleGGX_VNDF(const glm::vec3& N, const glm::vec3& V, float roughness, curandState& randState, float& pdfHalf)
{
	float a = roughness * roughness;
	float zeta1 = RandomFloat(randState);
	float zeta2 = RandomFloat(randState);

	//get v to tangent space
	glm::vec3 T, B;
	BuildTBN(T, B, N);
	glm::vec3 Vlocal = glm::vec3(glm::dot(V, T), glm::dot(V, B), glm::dot(V, N));

	//3.2
	glm::vec3 Vh = glm::normalize(glm::vec3(a * Vlocal.x, a * Vlocal.y, Vlocal.z));

	//4.1
	float lengthSq = Vh.x * Vh.x + Vh.y * Vh.y;
	glm::vec3 T1 = (lengthSq > 0) ? glm::vec3(-Vh.y, Vh.x, 0.0f) * glm::inversesqrt(lengthSq) : glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 T2 = glm::cross(Vh, T1);

	//4.2
	float r = sqrtf(zeta1);
	float phi = 2.0f * pi * zeta2;
	float t1 = r * cosf(phi);
	float t2 = r * sinf(phi);
	float s = 0.5f * (1.0f + Vh.z);
	t2 = (1.0f - s) * sqrtf(1.0f - (t1 * t1)) + (s * t2);

	//4.3
	glm::vec3 Nh = t1 * T1 + t2 * T2 + sqrtf(fmax(0.0f, 1.0f - t1*t1 - t2*t2)) * Vh;

	//3.4
	glm::vec3 Hlocal = glm::normalize(glm::vec3(a * Nh.x, a * Nh.y, fmax(Nh.z, 0.0f)));

	//world space
	glm::vec3 halfway = glm::normalize(Hlocal.x * T + Hlocal.y * B + Hlocal.z * N);

	//get pdf
	float NdotH = fmaxf(glm::dot(N, halfway), 0.0f);
	float NdotV = fmaxf(glm::dot(N, V), 0.0f);
	float VdotH = fmaxf(glm::dot(V, halfway), 0.0f);
	float D = D_GGX(NdotH, a);
	float G1 = 1.0f / (1.0f + LambdaGGX(NdotV, a));

	pdfHalf = (G1 * VdotH * D) / NdotV; //denom is V dot Z

	return halfway;
}

__device__ inline glm::vec3 SampleLambertian(const glm::vec3& N, curandState& randState, float& pdf)
{
	//cos(theta) = sqrt(1 - zeta1)
	//phi = 2pi * zeta2


	float zeta1 = RandomFloat(randState);
	float zeta2 = RandomFloat(randState);

	float r = sqrtf(zeta1);
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

	if (materialData.roughness < 0.04f) //fall back to mirror
	{
		glm::vec3 L = glm::reflect(-V, N);
		scattered = Ray(rec.p, L);
		attenuation = glm::vec3(1.0);   
		pdf = 1.0f;	
		return true;
	}

	
	float pdfHalf;
	glm::vec3 halfway;

	halfway = SampleGGX_VNDF(N, V, materialData.roughness, randState, pdfHalf);
	//halfway = SampleGGX(N, materialData.roughness, randState, pdfHalf);
	
	//reflect on the haldway vector to get sample vector
	glm::vec3 L = glm::reflect(-V, halfway);	

	if (glm::dot(L, N) <= 0.0f) return false;

	//while (glm::dot(L, N) <= 0.0f)
	//{
	//	halfway = SampleGGX(N, materialData.roughness, randState, pdfHalf);
	//	L = glm::reflect(-V, halfway);
	//}
	//if (glm::dot(L, N) <= 0.0f) return false; //ENERGY LOSS WARNING

	pdf = pdfHalf / (4.0f * fabsf(dot(V, halfway))); //changes of variables adds jacobian factor to pdf
	if (pdf < 1e-6f) return false;

	//glm::vec3 L = SampleLambertian(N, randState, pdf); 
	//if (pdf < 1e-6f || glm::dot(L, N) <= 0.0f)           
	//	return false;
	//glm::vec3 halfway = glm::normalize(V + L);

	//evalyate BRDF to find attenuation
	float NdotL = fmaxf(glm::dot(N, L), 0.0f);
	float NdotV = fmaxf(glm::dot(N, V), 0.0f);
	float NdotH = fmaxf(glm::dot(N, halfway), 0.0f);

	float D = D_GGX(NdotH, materialData.roughness * materialData.roughness);
	float G = G_SmithHeightCorrelated(NdotV, NdotL, materialData.roughness * materialData.roughness);

	//TEMP
	glm::vec3 F0 = mix(glm::vec3(0.04f), materialData.albedo, materialData.metallic); //still hack
	float VdotH = glm::clamp(glm::dot(V, halfway), 0.0f, 1.0f);
	glm::vec3 F = FresnelSchlick(VdotH, F0);

	glm::vec3 specular = (D * G * F) / (4.0f * NdotV * NdotL + 1e-6f);
	attenuation = specular; //just the brdf
	scattered = Ray(rec.p, L);

	return true;
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
	attenuation = materialData.albedo / pi;

	return true;
}	

/*
__device__ inline float SampleBurleyDistance(float u, float d)
{
	// Avoid u==0
	u = fmaxf(u, 1e-6f);

	float g = 1.0f + 4.0f * u * (2.0f * u + sqrtf(1.0f + 4.0f * u * u));   
	float c = powf(g, 1.0f / 3.0f);
	float r = d * (c + 1.0f / c - 2.0f);                           
	return r;                                                      
}
*/

/*
__device__ inline bool ScatterSubsurface(const MaterialData& materialData,
	curandState& randState,
	const Ray& ray,
	const HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf)
{
	const glm::vec3 N = rec.normal;
	const glm::vec3 A = materialData.albedo;
	const glm::vec3 tint = materialData.sssTint;
	const float s = materialData.sssRadius;

	// 1. Sample a radius and angle
	float zeta1 = RandomFloat(randState);
	float zeta2 = RandomFloat(randState);

	float r = glm::max(SampleBurleyDistance(zeta1, s), 1e-4f * s);
	float phi = 2.0f * pi * zeta2;

	// 2. Convert polar offset to a point on the surface
	glm::vec3 T, B;
	BuildTBN(T, B, N);
	glm::vec3 offset = r * (cosf(phi) * T + sinf(phi) * B);
	glm::vec3 pOut = rec.p + offset;

	// 3. Sample an outgoing direction
	float dirPdf;
	glm::vec3 L = SampleLambertian(N, randState, dirPdf);
	if (dirPdf < 1e-6f) return false;

	// 4. Calculate the BSSRDF value
	// The normalized diffusion profile Rd(r)
	float e1 = expf(-r / s);
	float e3 = expf(-r / (3.0f * s));
	float Rd = s * (e1 + e3) / (8.0f * pi * r); // This is the profile from Burley's paper

	float Fr = FresnelSchlick(glm::dot(N, -ray.direction()), materialData.refractionIndex);
	float Ft = 1.0f - Fr;

	// The attenuation should be the BSSRDF value
	attenuation = Ft * A * tint * (Rd/pi);

	// 5. Calculate the PDF
	// The PDF of sampling the radius r is r * R(r)
	float pr = (s * 0.25f) * (e1 + e3);
	float pdfPos = pr / (2.0f * pi * r);     
	pdf = pdfPos * dirPdf;                        
	if (pdf < 1e-6f || isnan(pdf)) return false;

	scattered = Ray(pOut + 1e-3f * N, L); // Small offset to avoid self-intersection
	return true;

}
*/

//sss
/*
if (materialData.type == MAT_SUBSURFACE)
{
	float wSpec = glm::clamp(materialData.metallic, 0.0f, 1.0f);
	float wRest = 1.0f - wSpec;

	float wSSS = glm::clamp(materialData.subsurface, 0.0f, 1.0f) * wRest;
	float wDiff = (1.0f - glm::clamp(materialData.subsurface, 0.0f, 1.0f)) * wRest;

	float sum = wSpec + wSSS + wDiff;
	if (sum <= 1e-6f) return false; //todo
	wSpec /= sum;
	wSSS /= sum;
	wDiff /= sum;

	if (r < wSpec)
	{
		bool ok = ScatterGGX(materialData, randState, ray, rec, attenuation, scattered, pdf);
		//pdf *= wSpec;
		return ok;
	}
	else if (r < wSpec + wSSS)
	{
		bool ok = ScatterSubsurface(materialData, randState, ray, rec, attenuation, scattered, pdf);
		//pdf *= wSSS;
		return ok;
	}
	else
	{
		bool ok = ScatterLambertian(materialData, randState, ray, rec, attenuation, scattered, pdf);
		//pdf *= wDiff;
		return ok;
	}
}
*/

//// 1.  Sample radial distance r from Burleys CDF
//float u = fmaxf(RandomFloat(randState), 1e-6f);
//float g = 1.f + 4.f * u * (2.f * u + sqrtf(1.f + 4.f * u * u));
//float c = powf(g, 1.f / 3.f);
//float r = materialData.sssRadius * (c + 1.f / c - 2.f);

//// 2.  Uniform azimuth
//float phi = 2.f * pi * RandomFloat(randState);

//// 3.  Offset in tangent plane
//glm::vec3 T, B; BuildTBN(T, B, rec.normal);
//glm::vec3 offset = r * (cosf(phi) * T + sinf(phi) * B);

//// 4.  Project back onto the real surface
//Ray probe(rec.p + offset + rec.normal * 1e-4f, -rec.normal);
//HitRecord h;
//if (!HitScene(scene, probe, Interval(0.0f, materialData.sssRadius * 4.f), h)) return false;

//xi = h.p;
//xiN = h.normal;

//// 5.  Burley profile value & pdf
//float s = materialData.sssRadius;
//r = fmaxf(r, s * 1e-4f);
//float e1 = expf(-r / s);
//float e3 = expf(-r / (3.f * s));
//float Rd = (e1 + e3) / (8.f * pi * r * s);    // Burley 
//pdfS = Rd;
//Sp = materialData.sssTint * glm::vec3(Rd);

//return true;
//

//VERSION 1
//float u = fmaxf(RandomFloat(randState), 1e-6f);
//float r, rcpPdf;
//SampleBurleyRadius(u, 1.0f / materialData.sssRadius, r, rcpPdf);

//float phi = 2.0f * pi * RandomFloat(randState);

//glm::vec3 T, B;  BuildTBN(T, B, rec.normal);
//glm::vec3 offset = r * (cosf(phi) * T + sinf(phi) * B);

////Project back onto the real surface
//Ray probe(rec.p + offset + rec.normal * 1e-4f, -rec.normal);
//HitRecord h;
//if (!HitScene(scene, probe, Interval(0.0f, materialData.sssRadius * 4.0f), h)) return false;
//xi = h.p;
//xiN = h.normal;

////assume exit point is just offset on the same surface?
////xi = rec.p + offset;
////xiN = rec.normal;

//pdfS = 1.0f / rcpPdf / r;   //mathematticaly shoud be 1 / rcpPdf? no r
//// 2) normalized diffusion profile Rd(r):
//float s = materialData.sssRadius;
//float rcpS = 1.0f / s;
//float e1 = expf(-r * rcpS);        // e^{-r/s}
//float e3 = expf(-r * (rcpS / 3.0f)); // e^{-r/(3s)}
//float Rd = (e1 + e3) * rcpS / (8.0f * pi * r);

////float Rd = pdfS; //mathermatcially should  be pdfS / r?
//Sp = materialData.sssTint * glm::vec3(Rd);
//return true;
