#include "../include/Material.h"

//Other helpers-----------------------------------------------

__device__  float FresnelMoment1(float invEta)
{
	float e = invEta;
	float e2 = e * e, e3 = e2 * e, e4 = e2 * e2, e5 = e3 * e2;
	if (e < 1.0f)
		return 0.45966f - 1.73965f * e + 3.37668f * e2 - 3.904945f * e3 + 2.49277f * e4 - 0.68441f * e5;
	else
		return -4.61686f + 11.1136f * e - 10.4646f * e2 + 5.11455f * e3 - 1.27198f * e4 + 0.12746f * e5;
}

__device__ float FrDielectricExact(float cosThetaI, float etaI, float etaT)
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


__device__ float FresnelSchlick(float cosT, float eta)
{
	float r0 = (1 - eta) / (1 + eta);
	r0 *= r0;
	return r0 + (1 - r0) * powf(1 - cosT, 5);
}

//for precomputed F0
__device__  glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& F0)
{
	return F0 + (glm::vec3(1.0f) - F0) * powf(1.0f - cosTheta, 5.0f);
}

__device__ float D_GGX(float NdotH, float alpha)
{
	float a2 = alpha * alpha;
	float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
	return a2 / (pi * denom * denom);
}

__device__ float G1_SchlickGGX(float NdotX, float k)
{
	return NdotX / (NdotX * (1.0f - k) + k);
}

__device__ float G_Smith(float NdotV, float NdotL, float roughness)
{
	float k = roughness + 1.0f;
	k = (k * k) / 8.0f;
	float gv = G1_SchlickGGX(NdotV, k);
	float gl = G1_SchlickGGX(NdotL, k);

	return gv * gl;
}

//isotropic GGX
__device__ float LambdaGGX(float cosTheta, float alpha)
{
	float a2 = alpha * alpha;
	float cos2 = cosTheta * cosTheta;
	return (-1.0f + sqrtf(1.0f + a2 * (1.0f - cos2) / cos2)) * 0.5f;
}

__device__ float G_SmithHeightCorrelated(float NdotV, float NdotL, float alpha)
{
	return 1.0f / (1.0f + LambdaGGX(NdotV, alpha) + LambdaGGX(NdotL, alpha));
}

__device__ glm::vec3 SampleGGX(const glm::vec3& N, float roughness, curandState& randState, float& pdfHalf)
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
__device__ glm::vec3 SampleGGX_VNDF(const glm::vec3& N, const glm::vec3& V, float roughness, curandState& randState, float& pdfHalf)
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
	glm::vec3 Nh = t1 * T1 + t2 * T2 + sqrtf(fmax(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

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

__device__ float BurleyRd(float r, float d)
{
	float e1 = std::exp(-r / d);
	float e3 = std::exp(-r / d * (1.0f / 3.0f));
	return ((e1 + e3)) / (8.0f * pi * d * r);
}

__device__ float BurleyRd(float r, float s, float l)
{
	float d = l / s;
	return BurleyRd(r, d);
}

__device__ float BurleyDiskPdf(float r, float s, float ell)
{
	return BurleyRd(r, s, ell);
}

__device__ glm::vec3 EvaluateDiffusionProfile(float distance, const SubsurfaceParams& params, int channel)
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
__device__ void SampleBurleyRadius(float u, float rcpS, float& r, float& rcpPdf)
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

__device__ void SampleDisneyRadius(float u, const SubsurfaceParams& params, float& r, float& pdf, int channel)
{
	//type1
	float A = params.albedo[channel];
	float s = 1.85f - A + 7.0f * powf(fabsf(A - 0.8f), 3.0f);
	SampleBurleyRadius(u, 1 / s, r, pdf); //note that ell = sssRadius is not effecting this
	r *= params.ell; //normalized -> radius 
}

__device__ bool SampleDisneySubsurface(const SubsurfaceParams& params,
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
	if (uA < 0.5f) //N
	{
		axisN = rec.normal; vx = T; vy = B;
	}
	else if (uA < 0.75f) //T
	{
		axisN = T;  vx = B;  vy = rec.normal;
	}
	else //B
	{
		axisN = B;  vx = rec.normal;  vy = T;
	}

	// 1. importance sample a radius (theta)
	float u = RandomFloat(randState);
	u = fminf(fmaxf(u, 1e-4f), 1.0f - 1e-4f);
	float r, pdfR;
	SampleDisneyRadius(u, params, r, pdfR, channel);

	// 2. Take uniform azimuthal angle
	float phi = 2.0f * pi * RandomFloat(randState);
	//float pdfPhi = 1.0f / (2.0f * pi); //no need, included in sampleDisneyRadius

	// 3. polar to cartesian
	glm::vec3 offset = r * (cosf(phi) * vx + sinf(phi) * vy);

	// 4. Probe to find intersections
	float rMax, tmp_inv;

	SampleDisneyRadius(0.999f, params, rMax, tmp_inv, channel);
	float l = 2.0f * sqrtf(fmaxf(0.0f, rMax * rMax - r * r)); //As in pbrt

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
		//atomicAdd(&noIntersection, 1);
		return false;
	}

	// 5. By PBRT: Pick one random intersection
	int nHits = hList.count;
	float u2 = fmaxf(RandomFloat(randState), 1e-6f);
	int pick = fminf(int(u2 * nHits), nHits - 1);

	const HitRecord& h = hList.hits[pick];

	xi = h.p;
	xiN = h.geoNormal;

	glm::vec3 N = axisN;
	glm::vec3 d = xi - rec.p;

	float dLocal[3] = { fabsf(dot(T, d)),	fabsf(dot(B, d)),	 fabsf(dot(N, d)) }; //d in exit space
	float nLocal[3] = { fabsf(dot(T, xiN)), fabsf(dot(B, xiN)),  fabsf(dot(N, xiN)) }; //xiN in exit space
	float rProj[3] = { std::sqrtf(dLocal[1] * dLocal[1] + dLocal[2] * dLocal[2]),
				   std::sqrtf(dLocal[2] * dLocal[2] + dLocal[0] * dLocal[0]),
				   std::sqrtf(dLocal[0] * dLocal[0] + dLocal[1] * dLocal[1]) };

	float A = params.albedo[channel];
	float s = 1.85f - A + 7.0f * powf(fabsf(A - 0.8f), 3.0f); //note this is artistic
	float ell = params.ell;

	float pMix = 0.0f;

	float contrib = BurleyDiskPdf(fmaxf(/*rProj[axis]*/ glm::length(d), 1e-6f), s, ell) /** nLocal[axis]*/;
	pMix += contrib;

	pdfS = pMix;
	if (!(pdfS > 0 && isfinite(pdfS))) return false;

	return true;
}

__device__ void BuildTBN(glm::vec3& T, glm::vec3& B, const glm::vec3 N)
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