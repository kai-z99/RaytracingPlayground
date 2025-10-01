#pragma once

#include "Material.cuh"

struct EnvSample 
{
    glm::vec3 wi;      // sampled direction (unit)
    float pdf_dir;     // pdf over solid angle
    glm::vec3 Le;      // environment radiance along wi
};

struct LightPointSample 
{
    glm::vec3 pos;     // point on light
    glm::vec3 normal;  // geometric normal at pos (unit)
    glm::vec3 Le;      // emitted radiance toward the shading point
    float pdf_area;    // pdf wrt AREA for the chosen light primitive
};

//samples point on skybox
__device__ inline EnvSample SampleEnvironment(const Scene& scene, curandState& RNG, const glm::vec3& backgroundColor)
{
    EnvSample s{};
    // Uniform over sphere
    float u1 = RandomFloat(RNG);
    float u2 = RandomFloat(RNG);
    float z = 1.0f - 2.0f * u1;
    float r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    float phi = 2.0f * pi * u2;
    s.wi = glm::normalize(glm::vec3(r * cosf(phi), r * sinf(phi), z));
    s.pdf_dir = 1.0f / (4.0f * pi);
    s.Le = backgroundColor; // replace with env lookup if you have one
    return s;
}

//sample a random light
__device__ inline const EmissiveGeom& SampleEmissive(const Scene& scene, curandState& rs)
{
    // Assumes scene.lightSet is non-null and count > 0
    const LightSetGPU& L = *scene.lightSet;
    float xi = RandomFloat(rs);

    // Binary search the CDF
    uint32_t lo = 0, hi = L.count - 1;
    while (lo < hi) 
    {
        uint32_t mid = (lo + hi) >> 1;
        if (xi > L.cdf[mid]) lo = mid + 1;
        else                 hi = mid;
    }
    return L.lights[lo];
}


// --- Sphere light: uniform over surface -----------------------------------
__device__ inline LightPointSample SampleSphereLight(const Scene& scene,
    const EmissiveGeom& L,
    curandState& RNG)
{
    LightPointSample s{};

    glm::vec3 c; float r; int _matID_unused;
    FetchSphere(scene.spheres, L.primIndex, c, r, _matID_unused);

    // uniform direction on sphere
    float u1 = RandomFloat(RNG);
    float u2 = RandomFloat(RNG);
    float z = 1.0f - 2.0f * u1;
    float t = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    float phi = 2.0f * pi * u2;

    glm::vec3 n = glm::normalize(glm::vec3(t * cosf(phi), t * sinf(phi), z));

    s.pos = c + r * n;
    s.normal = n;                                   // outward
    s.Le = scene.materials[L.matIndex].emissive.emission;
    s.pdf_area = 1.0f / (4.0f * pi * r * r);          
    return s;
}

// --- Triangle light: uniform barycentric ----------------------------------
__device__ inline LightPointSample SampleTriangleLight(const Scene& scene,
    const EmissiveGeom& L,
    curandState& RNG)
{
    LightPointSample s{};

    glm::vec3 p0, p1, p2; int _matID_unused;
    FetchTriangle(scene.tris, L.primIndex, p0, p1, p2, _matID_unused);

    // sample barycentrics uniformly
    float u = RandomFloat(RNG);
    float v = RandomFloat(RNG);
    if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }

    glm::vec3 p = p0 + u * (p1 - p0) + v * (p2 - p0);
    glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0)); // geometric normal

    s.pos = p;
    s.normal = n;
    s.Le = scene.materials[L.matIndex].emissive.emission;
    s.pdf_area = 1.0f / fmaxf(L.area, 1e-12f);      
    return s;
}

// --- Quad light: sample as two triangles (equal prob) ---------------------
__device__ inline LightPointSample SampleQuadLight(const Scene& scene,
    const EmissiveGeom& L,
    curandState& RNG)
{
    LightPointSample s{};

    glm::vec3 Q, u, v; int _matID_unused;
    FetchQuad(scene.quads, L.primIndex, Q, u, v, _matID_unused);

    // Uniform over the full parallelogram spanned by u and v
    float a = RandomFloat(RNG);
    float b = RandomFloat(RNG);

    s.pos = Q + a * u + b * v;
    s.normal = glm::normalize(glm::cross(u, v));
    s.Le = scene.materials[L.matIndex].emissive.emission;

    // area = |u x v|
    float area = glm::length(glm::cross(u, v));
    s.pdf_area = 1.0f / fmaxf(area, 1e-12f);
    return s;
}


//Sample a point on the light
__device__ inline LightPointSample SamplePointOnLight(const EmissiveGeom& L,
    const Scene& scene,
    curandState& rs)
{
    switch (L.type) {
    case PRIM_SPHERE: return SampleSphereLight(scene, L, rs);
    case PRIM_TRIANGLE:    return SampleTriangleLight(scene, L, rs);
    case PRIM_QUAD:   return SampleQuadLight(scene, L, rs);
    default: {
        LightPointSample s{}; s.pdf_area = 0.0f; return s;
    }
    }
}

__device__ inline glm::vec3 SampleDirectNEE(const Scene& scene,
    const HitRecord& rec,
    const MaterialGPU& m,
    const glm::vec3& wo,
    curandState& RNG,
    const glm::vec3& backgroundColor)
{
    // 1) Pick a light (area or environment). Here's a simple two-bucket sketch:
    //    - With probability p_env: sample environment
    //    - With probability (1 - p_env): sample a random emissive in scene
    // Choose some p_env so its cool or something
    const bool hasArea = scene.lightSet && scene.lightSet->count > 0;
    const float p_env = (hasArea)? 0.2f : 1.0f;
    
    const float xi = RandomFloat(RNG);

    // 2A) Environment sampling branch
    if (xi < p_env) {
        // Sample a direction from the environment distribution: returns wi, pdf_w
        EnvSample es = SampleEnvironment(scene, RNG, backgroundColor); // wi, pdf_dir, Le(wi)
        if (es.pdf_dir <= 0.0f) return glm::vec3(0.0f);

        float cosNo = fmaxf(0.0f, glm::dot(rec.shadingNormal, es.wi));
        if (cosNo <= 0.0f) return glm::vec3(0.0f);

        // Shadow ray to infinity: check if blocked immediately nearby (optional early occlusion for env if you support it)
        HitRecord tmp;
        if (HitScene(scene, Ray(rec.p, es.wi), Interval(0.001f, infinity), tmp)) return glm::vec3(0.0f);
            
        BSDFSample sample = ConstructAndEvaluateBSDF(m, wo, es.wi, rec, RNG);
        if (!sample.good) return glm::vec3(0.0f);
        glm::vec3 f = sample.f;
        // No MIS: weight is 1, so contribution = f * cos(theta_o) * Le / pdf_light
        return f * (cosNo * es.Le / (es.pdf_dir * p_env + 1e-6f));
    }

    if (!hasArea)
    {
        printf("This shouldnt happen.\n");
        return glm::vec3(0.0f);
    }

    // 2B) Area light sampling branch
    const EmissiveGeom& L = SampleEmissive(scene, RNG);
    LightPointSample ls = SamplePointOnLight(L, scene, RNG);  // pos, normal, Le, pdf_area (1/area_i)
    if (ls.pdf_area <= 1e-6f) return glm::vec3(0.0f);

    glm::vec3 toL = ls.pos - rec.p;
    float     dist2 = glm::dot(toL, toL);
    float     dist = sqrtf(dist2);
    glm::vec3 wi = toL / dist;

    // Shading cosine
    float cosNo = fmaxf(0.0f, glm::dot(rec.shadingNormal, wi));

	//2 sided emission
    float cosNl = glm::dot(ls.normal, -wi);
    float cosNl_abs = fabsf(cosNl);

    if (cosNo <= 1e-6f || cosNl_abs <= 1e-6f) return glm::vec3(0.0f);

    // Visibility test (shadow ray)
    HitRecord occ;
    if (HitScene(scene, Ray(rec.p, wi), Interval(0.001f, dist - 1e-2f), occ)) return glm::vec3(0.0f);

    // ----- PDF over solid angle -----
    float totalArea = fmaxf(scene.lightSet->totalArea, 1e-6f);
    float pdf_omega = (dist2 / fmaxf(cosNl_abs, 1e-6f)) / fmaxf(totalArea, 1e-6f);
    if (pdf_omega <= 1e-6f) return glm::vec3(0.0f);

    // BSDF eval
    BSDFSample sample = ConstructAndEvaluateBSDF(m, wo, wi, rec, RNG);
    if (!sample.good) return glm::vec3(0.0f);

    // e area-light path with (1 - p_env)
    float lightSelPdf = fmaxf(1.0f - p_env, 1e-6f);
    if (lightSelPdf <= 1e-6f) return glm::vec3(0.0f);

    // Final contribution
    glm::vec3 contrib = sample.f * (cosNo * ls.Le) / (pdf_omega * lightSelPdf);
    if (!isfinite(contrib.r) || !isfinite(contrib.g) || !isfinite(contrib.b)) return glm::vec3(0.0f);

    float t = 500;
    if (contrib.r > t || contrib.g > t || contrib.b > t) 
    {
        //return glm::vec3(0.0f);
        //printf("r: %f, g: %f, b: %f\n", contrib.r, contrib.g, contrib.b);
        //printf("sample.f.r: %f \n sample.f.g: %f \n sample.f.b: %f \n cosNo: %f \n ls.Le: %f \n pdf_omega: %f \n lightSelPdf: %f \n", sample.f.r, sample.f.g, sample.f.b, cosNo, ls.Le.r, pdf_omega, lightSelPdf);
    }

    return contrib;
}
