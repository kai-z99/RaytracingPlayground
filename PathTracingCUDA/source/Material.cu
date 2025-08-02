#include "../include/Material.h"

//#define DIPOLE
//#define REJECTION_SAMPLE
__managed__ unsigned int noIntersection = 0;
__managed__ unsigned int rejected = 0;
__managed__ unsigned int total = 0;
__managed__ unsigned int clampedPDFs = 0;
__managed__ unsigned int PDFs = 0;
__managed__ float radialSamplesSum = 0;
__managed__ float radialSamplesCount = 0;
__managed__ float expectedRadialAverage = 0;
__managed__ float uSum = 0;
__managed__ double  sssEnergySumR = 0.0;
__managed__ double  sssEnergySumG = 0.0;
__managed__ double  sssEnergySumB = 0.0;
__managed__ unsigned long long sssHitCount = 0;

__device__ inline float Luminance(const glm::vec3& col)
{
	const glm::vec3 lumaWeights(0.2126f, 0.7152f, 0.0722f);
	return glm::dot(col, lumaWeights);
}

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

//DIFFUSION PROFILES ---------------------------
//---------------------------------------------

//Jensen et al (2005)
__device__ inline float DipoleRd(float r, float sigmaS, float sigmaA, float eta)
{
	return 0.0f;
}

//Burley et al (2015)
//range: (0, inf)
__device__ inline float BurleyRd(float r, float s, float l) {
	float sl = s * r / l;
	float e1 = std::exp(-sl);
	float e3 = std::exp(-sl * (1.0f / 3.0f));
	return (s * (e1 + e3)) / (8.0f * pi * l * r);
}

//evaluate diffusion profiles
__device__ inline float EvaluateDiffusionProfile(float distance, const MaterialData& mat)
{
#ifdef DIPOLE
	// Dipole path
	return DipoleRd(r, mat.sigmaS, mat.sigmaA, mat.refractionIndex);
#else
	// Burley path
	float A = Luminance(mat.sssTint);
	float s = 1.85f - A + 7.0f * std::pow(std::abs(A - 0.8f), 3.0f);
	float l = mat.sssRadius;
	float Rd = BurleyRd(distance, s, l);
	return Rd;

#endif
}

//SAMPLERS ---------------------------
//---------------------------------------------

//?
__device__ inline void SampleDipoleRadius(float u, float sssRadius, float& r, float& pdfRd)
{
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


__device__ inline void SampleSSSRadius(float u, const MaterialData& mat, float& r, float& pdf, bool accum = false)
{
#ifdef DIPOLE
	// invoke dipole sample
	//SampleDipoleRadius(u, mat, r, pdf);
#else
	//type1
	float A = Luminance(mat.sssTint);
	float s = 1.85f - A + 7.0f * powf(fabsf(A - 0.8f), 3.0f);
	SampleBurleyRadius(u, 1 / s, r, pdf); //note that ell = sssRadius is not effecting this
	
	if (accum)
	{
		atomicAdd(&uSum, u);
		atomicAdd(&radialSamplesCount, 1.0f);
		atomicAdd(&radialSamplesSum, r);
		float respS = 1 / s;
		expectedRadialAverage = 2.5f * respS;
	}

	r *= mat.sssRadius; //normalized -> radius
	pdf = 1.0f / pdf; //function returns inverse pdf
	pdf /= mat.sssRadius; //change of variables
	pdf /= (fmaxf(r, 1e-6f)); 

#endif
}

__device__ bool SampleSubsurfaceDisk(const MaterialData& materialData,
	curandState& randState,
	const Scene& scene,
	const HitRecord& rec,
	glm::vec3& xi, //returned entry point
	glm::vec3& xiN, //return entry point normal
	float& pdfS)
{
	// 0. pick an axis
	glm::vec3 T, B;
	BuildTBN(T, B, rec.normal);
	float uA = RandomFloat(randState);
	uA = 1.0f;
	float pdfAxis;
	glm::vec3 axisN, vx, vy;

	if (uA < 0.5f)
	{
		pdfAxis = 0.5f;
		axisN = rec.normal; vx = T; vy = B;
	}
	else if (uA < 0.75f)
	{
		pdfAxis = 0.25f;
		axisN = T;  vx = B;  vy = rec.normal;
	}
	else
	{
		pdfAxis = 0.25f;
		axisN = B;  vx = rec.normal;  vy = T;
	}
	pdfAxis = 1.0f;

	// 1. importance sample a radius (theta)
	float u = RandomFloat(randState);
	u = fminf(fmaxf(u, 1e-4f), 1.0f - 1e-4f);
	float r, pdfR;
	SampleSSSRadius(u, materialData, r, pdfR, true);

	// 2. Take uniform azimuthal angle
	float phi = 2.0f * pi * RandomFloat(randState);
	float pdfPhi = 1.0f / (2.0f * pi); //no need, included in samplessradius

	// 3. tangent -> world space
	glm::vec3 offset = r * (cosf(phi)*vx + sinf(phi)*vy);

	// 4. Probe to find intersections
	float rMax, tmp_inv;
	SampleSSSRadius(0.999f, materialData, rMax, tmp_inv, false);
	float l = 2.0f * sqrtf(fmaxf(0.0f, rMax*rMax - r*r)); //As in pbrt
	//printf("rMax: %f, r: %f, l: %f\n", rMax, r, l);
	Ray probe(rec.p + (axisN * (l * 0.5f)) + offset, -axisN);
	HitList hList;
	if (!HitScene(scene, probe, Interval(0.0f, l), hList))
	{
		return false;
	}
		
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
		atomicAdd(&noIntersection, 1);
		return false;
	}
	
	// 5. By PBRT: Pick one random intersection
	int nHits = hList.count;
	float u2 = fmaxf(RandomFloat(randState), 1e-6f);
	int pick = fminf(int(u2 * nHits), nHits - 1);

	const HitRecord& h = hList.hits[pick];
	xi = h.p;
	xiN = h.normal;

	float cosTheta = fmaxf(fabsf(glm::dot(h.normal, axisN)), 1e-6f);
	pdfS = fmaxf(pdfR * pdfAxis * cosTheta / ((float)nHits), 1e-6f);

	atomicAdd(&PDFs, 1);
	if (pdfS == 1e-6f) atomicAdd(&clampedPDFs, 1);

	return true;
}

__device__ bool ScatterSubsurface(const MaterialData& materialData,
	curandState& randState,
	const Scene& scene,
	const Ray& ray,
	HitRecord& rec,
	glm::vec3& attenuation,
	Ray& scattered,
	float& pdf)
{
	//find where the light entered the surface xi
	glm::vec3 xi;
	glm::vec3 xiN;
	float pdfBssrdf;

	//do some rejection sampling to reduce variance
	atomicAdd(&total, 1);
	
#ifdef REJECTION_SAMPLE
	int tries = 4;
	while (tries > 0 && !SampleSubsurfaceDisk(materialData, randState, scene, rec, xi, xiN, Sp, pdfS))
	{
		tries--;
	}

	if (tries <= 0)
	{
		atomicAdd(&rejected, 1);
		return false;
	}
#else
	if (!SampleSubsurfaceDisk(materialData, randState, scene, rec, xi, xiN, pdfBssrdf)) return false;
#endif 
	//exit brdf
	float pdfBsdf;
	glm::vec3 L = SampleLambertian(xiN, randState, pdfBsdf);
	if (pdfBsdf < 1e-6f) return false;

	//build first fresnel term
	float VdotN = fmaxf(glm::dot(-ray.direction(), rec.normal), 0.0f);
	float F_o = FrDielectricExact(VdotN, 1.0f, materialData.refractionIndex); //(1 - Fo) term. for light leaving surface
	
	//build Sw
	float LdotxiN = fmaxf(glm::dot(L, xiN), 0.0f);
	float F_i = FrDielectricExact(LdotxiN, 1.0f, materialData.refractionIndex);
	float c = 1 - 2 * FresnelMoment1(1 / materialData.refractionIndex);
	float Sw = (1 - F_i) / (c * pi);

	//we need Sp, sample our diffusion profile.
	float distance = glm::length(xi - rec.p);
	glm::vec3 Sp = EvaluateDiffusionProfile(distance, materialData) * materialData.sssTint;

	//Sp * Sw * (1 - Fr)
	glm::vec3 bssrdfEvaluation = (1.0f - F_o) * Sp * Sw;

	attenuation = bssrdfEvaluation;
	scattered = Ray(xi + xiN * 1e-6f, L);
	pdf = pdfBssrdf * pdfBsdf;
	rec.normal = xiN;
	//printf("Sp: %f, %f, %f. PDF: %f\n", Sp.r, Sp.g, Sp.b, pdf);
	
	return true;
}