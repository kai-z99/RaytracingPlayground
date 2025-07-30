#include "../include/Material.h"

#define DIPOLE

__managed__ unsigned int rejected = 0;
__managed__ unsigned int total = 0;



__device__ inline void SampleDipoleRadius(float u, float sssRadius, float& r, float& pdfRd) 
{
	// (B) Newton-solve F(r)=u using evalDipoleRd and its CDF
	
	// Finally:
	//pdfRd = 2.0f * M_PI * r * evalDipoleRd(r, sigma_s, sigma_a, eta)
	//	/ normalization;
}

//DIFFUSION PROFILES---------------------------
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

__device__ inline float Luminance(const glm::vec3& col)
{
	const glm::vec3 lumaWeights(0.2126f, 0.7152f, 0.0722f);
	return glm::dot(col, lumaWeights);
}

//https://zero-radiance.github.io/post/sampling-diffusion/
// Performs sampling of a Normalized Burley diffusion profile in polar coordinates.
// 'u' is the random number (the value of the CDF): [0, 1).
// rcp(s) = 1 / ShapeParam = ScatteringDistance.
// 'r' is the sampled radial distance, s.t. (u = 0 -> r = 0) and (u = 1 -> r = Inf).
// rcp(Pdf) is the reciprocal of the corresponding PDF value.
__device__ inline void SampleBurleyDiffusionProfile(float u, float rcpS, float& r, float& rcpPdf)
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

__device__ inline float EvalSSSProfile(
	float r, const MaterialData& mat)
{
#ifdef USE_DIPOLE
	// Dipole path
	return DipoleRd(r, mat.sigmaS, mat.sigmaA, mat.refractionIndex);
#else
	// Burley path
	float A = Luminance(mat.sssTint);
	float s = 1.85f - A + 7.0f * std::pow(std::abs(A - 0.8f), 3.0f);
	float l = mat.sssRadius;
	float rMin = 1e-4f * mat.sssRadius;
	r = fmaxf(r, rMin);
	float Rd = BurleyRd(r, s, l);
	return Rd;

#endif
}

__device__ inline void SampleSSSRadius(float u, const MaterialData& mat, float& r, float& pdf)
{
#ifdef USE_DIPOLE
	// invoke dipole sample
	SampleDipoleRadius(u, mat, r, pdf);
#else
	// invoke Burley sample
	SampleBurleyDiffusionProfile( u, 1.0f / mat.sssRadius, r, /*rcpPdf*/ pdf);
	pdf = 1.0f / pdf;

	//float u = fmaxf(u, 1e-6f);
	//float g = 1.f + 4.f * u * (2.f * u + sqrtf(1.f + 4.f * u * u));
	//float c = powf(g, 1.f / 3.f);
	//float r = mat.sssRadius * (c + 1.f / c - 2.f);
	//pdf = 0.0f; //use Rd

#endif
}

__device__ bool SampleSubsurfaceDisk(const MaterialData& materialData,
	curandState& randState,
	const Scene& scene,
	const HitRecord& rec,
	glm::vec3& xi, //returned entry point
	glm::vec3& xiN, //return entry point normal
	glm::vec3& Sp,
	float& pdfS)
{
	//A. Sample radial distance & reciprocal PDF
	//float u = fmaxf(RandomFloat(randState), 1e-6f);
	//float r, rcpPdf;
	//SampleBurleyDiffusionProfile(u, 1.0f / materialData.sssRadius, r, rcpPdf);
	//float pdfBurley = 1.0f / rcpPdf;

	//B. Sample radial distance, pdf = Rd
	//float u = fmaxf(RandomFloat(randState), 1e-6f);
	//float g = 1.f + 4.f * u * (2.f * u + sqrtf(1.f + 4.f * u * u));
	//float c = powf(g, 1.f / 3.f);
	//float r = materialData.sssRadius * (c + 1.f / c - 2.f);	
	float u = RandomFloat(randState);
	float r, samplePdf;
	SampleSSSRadius(u, materialData, r, samplePdf);

	//C. uniform sample R
	//float r = sqrtf(RandomFloat(randState)) * materialData.sssRadius;

	// 2. Uniform azimuth
	float phi = 2.0f * pi * RandomFloat(randState);
	float pdfPhi = 1.0f / (2.0f * pi);

	// 3. Offset in tangent plane
	glm::vec3 T, B;
	BuildTBN(T, B, rec.normal);
	glm::vec3 offset = r * (cosf(phi) * T + sinf(phi) * B);

	// 4. Project back onto the real surface
	Ray probe(rec.p + (rec.normal * 1e-4f) + offset, -rec.normal);
	//HitList hList;
	//if (!HitScene(scene, probe, Interval(0.0f, materialData.sssRadius * 3.0f), hList)) return false;

	HitRecord h;
	if (!HitScene(scene, probe, Interval(0.0f, materialData.sssRadius * 4.f), h)) return false;

	//int nHits = hList.count;
	////pbrt design: pick one ----
	//float u2 = fmaxf(RandomFloat(randState), 1e-6f);
	//int pick = fminf(int(u2 * nHits), nHits - 1);
	//const HitRecord& h = hList.hits[pick];
	

	//TODO--- anti-firefly: evaulate all ----
	
	xi = h.p;
	xiN = h.normal;

	float Rd = EvalSSSProfile(r, materialData);
	Sp = materialData.sssTint * Rd;

	//if using burley sample, use burleyPDF instead.
	//pdfS = fmaxf(Rd /*/ float(nHits)*/, 1e-4f);

	pdfS = samplePdf;

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
	glm::vec3 Sp;
	float pdfS;

	//do some rejection sampling to reduce variance
	atomicAdd(&total, 1);
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

	float pdfF;
	glm::vec3 L = SampleLambertian(xiN, randState, pdfF);
	if (pdfF < 1e-6f) return false;

	float VdotN = fmaxf(glm::dot(-ray.direction(), rec.normal), 0.0f);
	float LdotxiN = fmaxf(glm::dot(L, xiN), 0.0f);
	float F_i = FresnelSchlick(VdotN, materialData.refractionIndex); 
	float F_o = FresnelSchlick(LdotxiN, materialData.refractionIndex);

	attenuation = Sp * (1.0f - F_i) * (1.0f - F_o) / pi;
	scattered = Ray(xi + xiN * 1e-4f, L);
	pdf = pdfS * pdfF;
	rec.normal = xiN;

	//printf("Sp: %f, %f, %f. PDF: %f\n", Sp.r, Sp.g, Sp.b, pdf);
	

	return true;
}

















//VERSION 2
// 1.  Sample radial distance r from Burleys CDF
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
//Ray probe(rec.p + (rec.normal * 1e-4f) + offset, -rec.normal);
//HitList hList;
//if (!HitScene(scene, probe, Interval(0.0f, materialData.sssRadius * 3.0f), hList)) return false;

//int nHits = hList.count;
//float u2 = fmaxf(RandomFloat(randState), 1e-6f);
//int pick = fminf(int(u2 * nHits), nHits - 1);
//const HitRecord& h = hList.hits[pick];
//
//xi = h.p;
//xiN = h.normal;

//glm::vec3 tint = materialData.sssTint;
//float A = Luminance(tint);
//float s = 1.85f - A + 7.0f * std::pow(std::abs(A - 0.8f), 3.0f);
//float l = materialData.sssRadius;
//r = fmaxf(r, 1e-6f * l);
//float Rd = BurleyRd(r, s, l);

//Sp = materialData.sssTint * glm::vec3(Rd);
//pdfS = Rd / (float)nHits;

//return true;