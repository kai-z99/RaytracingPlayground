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
extern __managed__ float dieSum;
extern __managed__ unsigned long long h1;
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
__device__ inline float D_GGX(float NdotH, float alpha);
__device__ inline float G_SmithHeightCorrelated(float NdotV, float NdotL, float alpha);
__device__ inline float FresnelSchlick(float cosT, float eta);
__device__ inline glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& F0);
__device__ inline float FrDielectricExact(float cosThetaI, float etaI, float etaT);
__device__ inline float FresnelMoment1(float invEta);
__device__ inline glm::vec3 EvaluateDiffusionProfile(float distance, const SubsurfaceParams& params, int channel);

//sampling
__device__ inline glm::vec3 SampleGGX_VNDF(const glm::vec3& N, const glm::vec3& V, float roughness, curandState& randState, float& pdfHalf);
__device__ inline glm::vec3 SampleGGX(const glm::vec3& N, float roughness, curandState& randState, float& pdfHalf);
__device__ inline bool SampleSubsurfaceDisk(const SubsurfaceParams& params, curandState& randState, const Scene& scene,const HitRecord& rec, glm::vec3& xi,  glm::vec3& xiN,  float& pdfS, int& channel);


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
	glm::vec3 N = glm::normalize(si.normal);
	glm::vec3 V = glm::normalize(wo);

	if (params.roughness < 0.04f) //fall back to mirror, temp
	{
		glm::vec3 L = glm::reflect(-V, N);
		sample.wi = L;
		sample.f = glm::vec3(1.0f) / fabsf(glm::dot(L, si.geoNormal)); //dirac delta, no cosine
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
		sample.good = false;
		return sample;
	};

	sample.pdf = pdfHalf / (4.0f * fabsf(dot(V, halfway))); //changes of variables adds jacobian factor to pdf

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
		sample.f = glm::vec3(1.0f) / fabsf(glm::dot(L, si.geoNormal)); //dirac delta, no cosine
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
	// Prefer exact dielectric Fresnel; fall back to your Schlick(cos, eta) if needed:
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

	// PDF for wi given wo (Eq. 9.37), when you sample m from the VNDF
	float dwm_dwi = fabsf(IdotM) / fmaxf(denomJac * denomJac, 1e-12f);
	float pdf = pdf_m * dwm_dwi;

	// Pack sample
	sample.wi = L;
	sample.f = ft;
	sample.pdf = pdf;
	sample.good = (pdf > 0.f) && (isfinite(ft.x) && isfinite(ft.y) && isfinite(ft.z));
	return sample;
}

__device__ inline BSDFSample SampleDielectricBSDF(const DielectricParams& params, const glm::vec3& wo, const HitRecord& si, curandState& RNG)
{
	BSDFSample sample;

	bool frontFace = glm::dot(-wo, si.geoNormal) < 0.0f;          
	glm::vec3 N = si.normal;

	// Indices for this side
	float etaI = frontFace ? 1.0f : params.eta;
	float etaT = frontFace ? params.eta : 1.0f;
	float eta = etaI / etaT;                            // for glm::refract

	// Exact Fresnel
	float cosThetaI = fabsf(glm::dot(wo, N));
	float R = FrDielectricExact(cosThetaI, etaI, etaT);  // returns 1 exactly in TIR
	float u = RandomFloat(RNG);

	MicrofacetParams p;
	p.albedo = glm::vec3(1.0f);
	p.metallic = 1.0f;
	p.roughness = params.roughness;
	p.eta = params.eta;

	glm::vec3 wi;
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
	}
}

//BSSRDF-----------------------------------------------------------

__device__ inline BSSRDFSample SampleSubsurfaceBSSRDF(const SubsurfaceParams& params, const HitRecord& si, const glm::vec3 wo, curandState& RNG, const Scene& scene)
{
	BSSRDFSample sample;
	sample.good = false;


	//find where the light entered the surface xi
	glm::vec3 xi;
	glm::vec3 xiN;
	float pdfBssrdf;
	int channel;

	//atomicAdd(&total, 1);

	if (!SampleSubsurfaceDisk(params, RNG, scene, si, xi, xiN, pdfBssrdf, channel)) return sample;

	//exit brdf
	HitRecord r;
	r.normal = xiN;
	LambertParams p;
	p.albedo = glm::vec3(1.0f);
	glm::vec3 dummyWo = glm::vec3(1.0f);
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

	//Sp * Sw * (1 - Fr)
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

	return SampleSubsurfaceBSSRDF(m.subsurface, si, wo, RNG, scene);
}


//Other helpers-----------------------------------------------

__device__ inline float FresnelMoment1(float invEta)
{
	float e = invEta;
	float e2 = e * e, e3 = e2 * e, e4 = e2 * e2, e5 = e3 * e2;
	if (e < 1.0f)
		return 0.45966f - 1.73965f * e + 3.37668f * e2 - 3.904945f * e3 + 2.49277f * e4 - 0.68441f * e5;
	else
		return -4.61686f + 11.1136f * e - 10.4646f * e2 + 5.11455f * e3 - 1.27198f * e4 + 0.12746f * e5;
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

	pdfHalf = (G1 * VdotH * D) / NdotV; //pbrt eq 9.23

	return halfway;
}

__device__ inline float BurleyRd(float r, float d)
{
	float e1 = std::exp(-r / d);
	float e3 = std::exp(-r / d * (1.0f / 3.0f));
	return ((e1 + e3)) / (8.0f * pi * d * r);
}

__device__ inline float BurleyRd(float r, float s, float l)
{
	float d = l / s;
	return BurleyRd(r, d);
}

__device__ inline float BurleyDiskPdf(float r, float s, float ell)
{
	return BurleyRd(r, s, ell);
}

__device__ inline glm::vec3 EvaluateDiffusionProfile(float distance, const SubsurfaceParams& params, int channel)
{
	// Burley
	float A = params.albedo[channel];
	float s = 1.85f - A + 7.0f * std::pow(std::abs(A - 0.8f), 3.0f);
	float l = params.ell;
	float Rd = BurleyRd(distance, s, l);
	return Rd * params.albedo; //!!??
}

//https://zero-radiance.github.io/post/sampling-diffusion/
// Performs sampling of a Normalized Burley diffusion profile in polar coordinates.
// 'u' is the random number (the value of the CDF): [0, 1).
// rcp(s) = 1 / ShapeParam = ScatteringDistance.
// 'r' is the sampled radial distance, s.t. (u = 0 -> r = 0) and (u = 1 -> r = Inf).
// rcp(Pdf) is the reciprocal of the corresponding PDF value.
__device__ inline void SampleBurleyRadius(float u, float rcpS, float& r, float& rcpPdf)
{
	const float LOG2_E = 1.44269504089f;
	u = 1.0f - u; // Convert CDF to CCDF; the resulting value of (u != 0)

	float g = 1.0f + (4.0f * u) * (2.0f * u + sqrtf(1.0f + (4.0f * u) * u));
	float n = exp2f(log2f(g) * (-1.0f / 3.0f));                    // g^(-1/3)
	float p = (g * n) * n;                                   // g^(+1/3)
	float c = 1.0f + p + n;                                     // 1 + g^(+1/3) + g^(-1/3)
	float x = (3.0f / LOG2_E) * log2f(c / (4.0f * u));           // 3 * Log[c / (4 * u)]

	// x      = s * r
	// exp_13 = Exp[-x/3] = Exp[-1/3 * 3 * Log[c / (4 * u)]]
	// exp_13 = Exp[-Log[c / (4 * u)]] = (4 * u) / c
	// exp_1  = Exp[-x] = exp_13 * exp_13 * exp_13
	// expSum = exp_1 + exp_13 = exp_13 * (1 + exp_13 * exp_13)
	// rcpExp = rcp(expSum) = c^3 / ((4 * u) * (c^2 + 16 * u^2))
	float rcpExp = ((c * c) * c) / ((4.0f * u) * ((c * c) + (4.0f * u) * (4.0f * u)));

	r = x * rcpS;
	rcpPdf = (8.0f * pi * rcpS) * rcpExp; // (8 * Pi) / s / (Exp[-s * r / 3] + Exp[-s * r])
}

__device__ inline void SampleSSSRadius(float u, const SubsurfaceParams& params, float& r, float& pdf, int channel, bool accum = false)
{
	//type1
	float A = params.albedo[channel];
	float s = 1.85f - A + 7.0f * powf(fabsf(A - 0.8f), 3.0f);
	SampleBurleyRadius(u, 1 / s, r, pdf); //note that ell = sssRadius is not effecting this

	if (accum)
	{
		//atomicAdd(&uSum, u);
		//atomicAdd(&radialSamplesCount, 1.0f);
		//atomicAdd(&radialSamplesSum, r);
		float respS = 1 / s;
		expectedRadialAverage = 2.5f * respS;
	}

	r *= params.ell; //normalized -> radius 
}

__device__ inline bool SampleSubsurfaceDisk(const SubsurfaceParams& params,
	curandState& randState,
	const Scene& scene,
	const HitRecord& rec,
	glm::vec3& xi, //returned entry point
	glm::vec3& xiN, //return entry point normal
	float& pdfS,
	int& channel)
{
	//choose a channel [0,1,2] = [r,g,b]
	float u3 = fmaxf(RandomFloat(randState), 1e-6f);
	channel = fminf(int(u3 * 3), 3 - 1);

	// 0. pick an axis
	glm::vec3 T, B;
	BuildTBN(T, B, rec.normal);

	float uA = RandomFloat(randState);
	glm::vec3 axisN, vx, vy;
	int axis;
	if (uA < 0.5f) //N
	{
		axisN = rec.normal; vx = T; vy = B;
		axis = 2;
	}
	else if (uA < 0.75f) //T
	{
		axisN = T;  vx = B;  vy = rec.normal;
		axis = 0;
	}
	else //B
	{
		axisN = B;  vx = rec.normal;  vy = T;
		axis = 1;
	}

	// 1. importance sample a radius (theta)
	float u = RandomFloat(randState);
	u = fminf(fmaxf(u, 1e-4f), 1.0f - 1e-4f);
	float r, pdfR;
	SampleSSSRadius(u, params, r, pdfR, channel, true);

	// 2. Take uniform azimuthal angle
	float phi = 2.0f * pi * RandomFloat(randState);
	float pdfPhi = 1.0f / (2.0f * pi); //no need, included in samplessradius

	// 3. polar to cartesian
	glm::vec3 offset = r * (cosf(phi) * vx + sinf(phi) * vy);

	// 4. Probe to find intersections
	float rMax, tmp_inv;
	SampleSSSRadius(0.999f, params, rMax, tmp_inv, channel, false);
	//printf("Rmax: %f\n", rMax);
	//printf("max:%f\n", rMax);
	float l = 2.0f * sqrtf(fmaxf(0.0f, rMax * rMax - r * r)); //As in pbrt
	//printf("rMax: %f, r: %f, l: %f\n", rMax, r, l);
	Ray probe(rec.p + (axisN * (l * 0.5f)) + offset, -axisN);
	HitList hList;
	if (!HitScene(scene, probe, Interval(0.0f, l), hList))
	{
		return false;
	}

	//if (hList.count > 1) printf("inital hits: %d\n", hList.count);
	//only keep intesection with same material
	int keep = 0;
	for (int i = 0; i < hList.count; ++i)
	{
		if (hList.hits[i].matDataID == rec.matDataID)
		{
			hList.hits[keep++] = hList.hits[i];
		}
	}

	hList.count = keep;
	if (hList.count == 0)
	{
		//atomicAdd(&noIntersection, 1);
		return false;
	}

	// 5. By PBRT: Pick one random intersection
	int nHits = hList.count;
	float u2 = fmaxf(RandomFloat(randState), 1e-6f);
	int pick = fminf(int(u2 * nHits), nHits - 1);

	const HitRecord& h = hList.hits[pick];
	//HitRecord h;
	//if (!HitScene(scene, probe, Interval(0.0f, l), h))
	//{
	//	return false;
	//}

	xi = h.p;
	xiN = h.geoNormal;

	//printf("nHits: %d\n", nHits);

	glm::vec3 N = axisN;
	glm::vec3 d = xi - rec.p;

	float dLocal[3] = { fabsf(dot(T, d)),	fabsf(dot(B, d)),	 fabsf(dot(N, d)) }; //d in exit space
	float nLocal[3] = { fabsf(dot(T, xiN)), fabsf(dot(B, xiN)),  fabsf(dot(N, xiN)) }; //xiN in exit space

	//printf("xiN: %f,%f,%f   T: %f,%f,%f    B: %f,%f,%f  N: %f,%f,%f\n", xiN.x, xiN.y, xiN.z, T.x, T.y, T.z, B.x, B.y, B.z, N.x, N.y, N.z );
	//printf("dLocal: %f,%f,%f\n", dLocal[0], dLocal[1], dLocal[2]);

	float rProj[3] = { std::sqrtf(dLocal[1] * dLocal[1] + dLocal[2] * dLocal[2]),
				   std::sqrtf(dLocal[2] * dLocal[2] + dLocal[0] * dLocal[0]),
				   std::sqrtf(dLocal[0] * dLocal[0] + dLocal[1] * dLocal[1]) };

	float A = params.albedo[channel];
	float s = 1.85f - A + 7.0f * powf(fabsf(A - 0.8f), 3.0f); //note this is artistic
	float ell = params.ell;

	float axisPdfs[3] = { 0.f, 0.f, 1.0f }; // T , B , N
	float channelPdf = 1.0f / 3.0f;
	float pMix = 0.0f;

	//printf("x: %f, y: %f, z: %f\n", rProj[0], rProj[1], rProj[2]);

	//for (int axis = 0; axis < 3; axis++)
	//{
	//	for (int ch = 0; ch < 3; ch++)
	//	{
	//		float contrib = BurleyDiskPdf(fmaxf(/*rProj[axis]*/ glm::length(d), 1e-6f), s, ell) /* nLocal[axis]*/ * axisPdfs[axis] * channelPdf;
	//		pMix += contrib;
	//	}
	//}

	float contrib = BurleyDiskPdf(fmaxf(/*rProj[axis]*/ glm::length(d), 1e-6f), s, ell) /** nLocal[axis]*/;
	pMix += contrib;
	//61 39
	//63 40

	pdfS = pMix;
	if (!(pdfS > 0 && isfinite(pdfS))) return false;

	//atomicAdd(&PDFs, 1);
	//if (pdfS == 1e-8f) atomicAdd(&clampedPDFs, 1);

	return true;
}

