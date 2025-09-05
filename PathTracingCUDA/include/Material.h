#pragma once

#include "Hittable.h"
#include "Scene.h"
#include "CudaHelper.h"

//
//----------
// 
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
	float eta;
};

struct DielectricParams
{
	float eta;
	float roughness;
};

struct SubsurfaceParams
{
	glm::vec3 albedo;
	float ell;
	float eta;
	float roughness;
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
	glm::vec3 wi;
	float pdf;
	bool good;
};

//helpers
__device__ float D_GGX(float NdotH, float alpha);
__device__ float G_SmithHeightCorrelated(float NdotV, float NdotL, float alpha);
__device__ float FresnelSchlick(float cosT, float eta);
__device__ glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& F0);
__device__ float FrDielectricExact(float cosThetaI, float etaI, float etaT);
__device__ float FresnelMoment1(float invEta);
__device__ glm::vec3 EvaluateDiffusionProfile(float distance, const SubsurfaceParams& params, int channel);
__device__ void BuildTBN(glm::vec3& T, glm::vec3& B, const glm::vec3 N);


//sampling helpers
__device__ glm::vec3 SampleGGX_VNDF(const glm::vec3& N, const glm::vec3& V, float roughness, curandState& randState, float& pdfHalf);
__device__ glm::vec3 SampleGGX(const glm::vec3& N, float roughness, curandState& randState, float& pdfHalf);
__device__ bool SampleDisneySubsurface(const SubsurfaceParams& params, curandState& randState, const Scene& scene,const HitRecord& rec, glm::vec3& xi,  glm::vec3& xiN,  float& pdfS, int& channel);


//BSDF SAMPLERS----------------------
__device__ inline BSDFSample SampleLambertBRDF(const LambertParams& params, const glm::vec3& wo, const HitRecord& si, curandState& RNG)
{
	BSDFSample sample;

	//bxdf -> bsdf local frame
	glm::vec3 T, B;
	BuildTBN(T, B, si.normal);

	float zeta1 = RandomFloat(RNG);
	float zeta2 = RandomFloat(RNG);

	float r = sqrtf(zeta1);
	float phi = 2.0f * pi * zeta2;
	float x = r * cosf(phi);
	float y = r * sinf(phi);
	float z = sqrtf(1.0f - zeta1); //cos theta

	//wi
	glm::vec3 L = glm::normalize((x * T) + (y * B) + (z * si.normal));
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

//torrence-sparrow
__device__ inline BSDFSample SampleGGXMicrofacetBRDF(const MicrofacetParams& params, const glm::vec3& wo, const HitRecord& si, curandState& RNG)
{
	BSDFSample sample;
	sample.good = false;
	glm::vec3 N = glm::normalize(si.normal);
	glm::vec3 V = glm::normalize(wo);

	if (params.roughness < 0.04f) //fall back to mirror, temp
	{
		glm::vec3 L = glm::reflect(-V, N);
		sample.wi = L;
		sample.f = glm::vec3(1.0f) / fabsf(glm::dot(L, si.normal)); //dirac delta, no cosine
		sample.pdf = 1.0f;
		sample.isTransmission = false;
		sample.good = true;
		return sample;
	}

	float pdfHalf;
	glm::vec3 halfway;

	halfway = SampleGGX_VNDF(N, V, params.roughness, RNG, pdfHalf);
	//halfway = SampleGGX(N, params.roughness, RNG, pdfHalf);

	//reflect on the halfway vector to get sample vector
	glm::vec3 L = glm::reflect(-V, halfway);

	if (glm::dot(L, N) <= 0.0f) 
	{
		return sample;
	};

	sample.pdf = pdfHalf / (4.0f * fabsf(glm::dot(V, halfway))); //changes of variables adds jacobian factor to pdf

	//evalyate BRDF to find attenuation
	float NdotL = fmaxf(glm::dot(N, L), 0.0f);
	float NdotV = fmaxf(glm::dot(N, V), 0.0f);
	float NdotH = fmaxf(glm::dot(N, halfway), 0.0f);

	float D = D_GGX(NdotH, params.roughness * params.roughness);
	float G = G_SmithHeightCorrelated(NdotV, NdotL, params.roughness * params.roughness);

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


__device__ inline BSDFSample SampleGGXMicrofacetBTDF(const MicrofacetParams& params, const glm::vec3& wo, const HitRecord& si, curandState& RNG)
{
	BSDFSample sample;
	sample.good = false;
	sample.isTransmission = true;

	//geometry normal
	glm::vec3 N = glm::normalize(si.normal);
	glm::vec3 V = glm::normalize(wo);

	// Require opposite hemispheres for transmission
	float cosNoV = glm::dot(N, V);
	
	float etaOutside = 1.0f;
	float etaInside = params.eta;
	bool entering = cosNoV > 0.0f;
	float etaI = entering ? etaOutside : etaInside;
	float etaT = entering ? etaInside : etaOutside;
	float eta = etaI / etaT;               // relative IOR from wo-side to wi-side
	float etap = etaT / etaI;               // relative IOR toward wi

	if (params.roughness < 0.004f) //perfect refraction
	{
		glm::vec3 L = glm::refract(-V, N, eta);
		sample.wi = L;
		sample.f = glm::vec3(1.0f) / fabsf(glm::dot(L, si.normal)); //dirac delta, no cosine
		sample.pdf = 1.0f;
		sample.isTransmission = true;
		sample.good = true;
		return sample;
	}

	// VNDF sample the microfacet normal
	float pdf_m = 0.f;
	glm::vec3 halfway = SampleGGX_VNDF(N, V, params.roughness, RNG, pdf_m);

	glm::vec3 L = glm::refract(-V, halfway, eta);
	float cosNoL = glm::dot(N, L);

	//diacrd backfacing microfacets
	if (glm::dot(halfway, L) * cosNoL < 0 || glm::dot(halfway, V) * cosNoV < 0) return sample;
	if (cosNoL > 0.0f) return sample;

	// Fresnel for dielectrics at the microfacet
	float VdotM = fmaxf(glm::dot(V, halfway), 0.0f);
	// Prefer exact dielectric Fresnel
	float R = FrDielectricExact(VdotM, etaI, etaT);
	float T = 1.0f - R;

	// Microfacet terms
	float alpha = params.roughness * params.roughness;
	float NdotM = fmaxf(glm::dot(N, halfway), 0.0f);
	float NdotV = fmaxf(glm::dot(N, V), 0.0f);
	float NdotL = fmaxf(glm::dot(N, L), 0.0f);
	float IdotM = glm::dot(L, halfway);

	float D = D_GGX(NdotM, alpha);
	float G = G_SmithHeightCorrelated(NdotV, fabsf(glm::dot(N, L)), alpha);

	// BTDF value (Eq. 9.40 in PBRT v4)
	float denomJac = (IdotM + VdotM / etap);
	float denom = (denomJac * denomJac) * fabsf(NdotV) * fabsf(glm::dot(N, L));
	glm::vec3 ft = glm::vec3((D * G * T) * fabsf(IdotM * VdotM) / fmaxf(denom, 1e-12f));

	// PDF for wi given wo (Eq. 9.37), 
	float dwm_dwi = fabsf(IdotM) / fmaxf(denomJac * denomJac, 1e-12f);
	float pdf = pdf_m * dwm_dwi;

	sample.wi = L;
	sample.f = ft;
	sample.pdf = pdf;
	sample.good = (pdf > 0.f) && (isfinite(ft.x) && isfinite(ft.y) && isfinite(ft.z));
	return sample;
}

__device__ inline BSDFSample SampleDielectricBSDF(const DielectricParams& params, const glm::vec3& wo, const HitRecord& si, curandState& RNG)
{
	bool frontFace = glm::dot(-wo, si.geoNormal) < 0.0f;          
	glm::vec3 N = si.normal;

	// Indices for this side
	float etaI = frontFace ? 1.0f : params.eta;
	float etaT = frontFace ? params.eta : 1.0f;
	//float eta = etaI / etaT; 

	// Exact Fresnel
	float cosThetaI = fabsf(glm::dot(wo, N));
	float R = FrDielectricExact(cosThetaI, etaI, etaT);  // returns 1 exactly in TIR
	float u = RandomFloat(RNG);

	MicrofacetParams p;
	p.albedo = glm::vec3(1.0f);
	p.metallic = 1.0f;
	p.roughness = params.roughness;
	p.eta = params.eta;

	if (u < R) 
	{
		return SampleGGXMicrofacetBRDF(p, wo, si, RNG);
	}
	else 
	{
		return SampleGGXMicrofacetBTDF(p, wo, si, RNG);
	}
}

__device__ inline BSDFSample SampleSubsurfaceBSDF(const SubsurfaceParams& params, const glm::vec3& wo, const HitRecord& si, curandState& RNG)
{
	DielectricParams p;
	p.eta = params.eta;
	p.roughness = params.roughness;
	return SampleDielectricBSDF(p, wo, si, RNG);
}

__device__ inline BSDFSample SampleEmissiveBSDF(const EmissiveParams& params, const glm::vec3& wo, const HitRecord& si, curandState& RNG)
{
	BSDFSample s;
	s.good = false;
	return s;
}


__device__ inline BSDFSample ConstructAndSampleBSDF(const MaterialGPU& m, const glm::vec3& wo, const HitRecord& si, curandState& RNG)
{
	switch (m.tag)
	{
	case LAMBERT:	 return SampleLambertBRDF	(m.lambert, wo, si, RNG);
	case MICROFACET: return SampleGGXMicrofacetBRDF(m.microfacet, wo, si, RNG);
	case DIELECTRIC: return SampleDielectricBSDF(m.dielectric, wo, si, RNG);
	case SUBSURFACE: return SampleSubsurfaceBSDF(m.subsurface, wo, si, RNG);
	case EMISSIVE:   return SampleEmissiveBSDF(m.emissive, wo, si, RNG);
	default:
		BSDFSample s;
		s.good = false;
		return s;
	}
}

//BSSRDF-----------------------------------------------------------

__device__ inline BSSRDFSample SampleSeparableBSSRDF(const SubsurfaceParams& params, const HitRecord& si, const glm::vec3 wo, curandState& RNG, const Scene& scene)
{
	BSSRDFSample sample;
	sample.good = false;

	//find wherea and dir the light entered the surface xi
	glm::vec3 xi;
	glm::vec3 xiN;
	float pdfBssrdf;
	int channel;

	//atomicAdd(&total, 1);

	//get an entrance point based on Burley's profile
	if (!SampleDisneySubsurface(params, RNG, scene, si, xi, xiN, pdfBssrdf, channel)) return sample;

	//exit brdf: lambertian
	HitRecord r;
	r.normal = xiN;
	LambertParams p;
	p.albedo = glm::vec3(1.0f);
	glm::vec3 dummyWo = glm::vec3(1.0f); //dont care about the exit dir for lambert
	BSDFSample adapterSample = SampleLambertBRDF(p, dummyWo, r, RNG);
	if (!adapterSample.good) return sample;

	//build Sw
	float LdotxiN = fmaxf(glm::dot(adapterSample.wi, xiN), 0.0f);
	float F_i = FrDielectricExact(LdotxiN, 1.0f, params.eta);
	float c = 1 - 2 * FresnelMoment1(1 / params.eta);
	float Sw = (1 - F_i) / (c * pi);
	if (!isfinite(Sw)) return sample;

	//we need Sp, sample our diffusion profile.
	float distance = glm::length(xi - si.p);
	glm::vec3 Sp = EvaluateDiffusionProfile(distance, params, channel);
	if (!isfinite(Sp.r) || !isfinite(Sp.g) || !isfinite(Sp.b)) return sample;

	//Sp * Sw * (1 - Fr) which is folded into dielctric bsdf
	glm::vec3 bssrdfEvaluation = Sp * Sw;

	sample.f = bssrdfEvaluation;
	sample.pi = xi;
	sample.pdf = pdfBssrdf * adapterSample.pdf;
	sample.ni = xiN;	
	sample.wi = adapterSample.wi;
	sample.good = true;

	return sample;
}

__device__ inline BSSRDFSample ConstructAndSampleBSSRDF(const MaterialGPU& m, const HitRecord& si, const glm::vec3& wo, curandState& RNG, const Scene& scene)
{
	if (m.tag != SUBSURFACE) return BSSRDFSample();

	return SampleSeparableBSSRDF(m.subsurface, si, wo, RNG, scene);
}