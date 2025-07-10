#include "../include/ScenePresets.h"
#include "../include/SceneBuilder.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <random>

Scene* Scenes::RayTracingInOneWeekend(int seed)
{
    SceneBuilder wb;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);

    //floor
    LambertianMaterial groundMat(glm::vec3(1.0f));
    wb.AddQuad(
        glm::vec3(-150.0f, 0.0f, -150.0f),
        glm::vec3(0.0f, 0.0f, 300.0f),
        glm::vec3(300.0f, 0.0f, 0.0f),
        groundMat);

    //random balls
    for (int a = -11; a < 11; ++a)
    {
        for (int b = -11; b < 11; ++b)
        {
            float choose = U(rng);
            glm::vec3 center = glm::vec3(a + 0.9f * U(rng), 0.2f, b + 0.9f * U(rng));
            if (glm::length(center - glm::vec3(4, 0.2f, 0)) < .9f) continue;

            if (choose < .8f)
            {
                // diffuse
                LambertianMaterial m(glm::vec3(U(rng) * U(rng), U(rng) * U(rng), U(rng) * U(rng)));
                wb.AddSphere(center, 0.2f, m);
            }
            else if (choose < .95f)
            {                // metal
                MetalMaterial m(
                    glm::vec3(.5f + .5f * U(rng), .5f + .5f * U(rng), .5f + .5f * U(rng)),
                    .5f * U(rng));

                wb.AddSphere(center, 0.2f, m);
            }
            else
            {                                // glass
                DialectricMaterial m(1.5f);
                wb.AddSphere(center, 0.2f, m);
            }
        }
    }

    //3 large spheres
    DialectricMaterial glass(1.5f);
    wb.AddSphere(glm::vec3(0, 1, 0), 1.0f, glass);

    LambertianMaterial lam(glm::vec3(0.4f, 0.2f, 0.1f));
    wb.AddSphere(glm::vec3(-4, 1, 0), 1.0f, lam);

    MetalMaterial metal(glm::vec3(0.7f, 0.6f, 0.5f));
    wb.AddSphere(glm::vec3(4, 1, 0), 1.0f, metal);

    return wb.Build();
}

Scene* Scenes::KaisScene(int seed)
{
    SceneBuilder sb;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);
    std::normal_distribution<float> N();

    for (int i = 0; i < 30; i++)
    {
        for (int j = 0; j < 30; j++)
        {
            sb.AddSphere(glm::vec3((float)(1.5f * i), 0.85f, (float)(1.5f * j)), 0.55f, MetalMaterial(glm::vec3(U(rng), U(rng), U(rng))));
        }
        
    }
    
    sb.AddQuad(
        glm::vec3(-300.0f, 0.0f, -300.0f), glm::vec3(0.0f, 0.0f, 600.0f), glm::vec3(600.0f, 0.0f, 0.0f), LambertianMaterial());

    return sb.Build();
}

Scene* Scenes::TriangleTestScene(int seed)
{
    SceneBuilder sb;

    // -------------------------------------------------------------------------
    // Random-number helpers
    // -------------------------------------------------------------------------
    std::mt19937 rng(seed);

    std::uniform_real_distribution<float> distPos(-80.0f, 80.0f);   // X, Y, Z range
    std::uniform_real_distribution<float> distSize(2.4f, 4.5f);     // side length
    std::uniform_real_distribution<float> distColor(0.2f, 1.0f);    // albedo tint
    std::uniform_real_distribution<float> distAngle(0.0f, glm::pi<float>()); // rotation
    std::uniform_real_distribution<float> distAxis(-1.0f, 1.0f);    // rotation axis
    std::uniform_real_distribution<float> choose(0.0f, 1.0f);  

    const int tetraCount = 10'000;  
    const float h = glm::sqrt(2.0f) / 2.0f;       // height of equilateral tri
    const float H = glm::sqrt(6.0f) / 3.0f;       // apex height

    const glm::vec3 v0(0.0f, 0.0f, 0.0f);
    const glm::vec3 v1(1.0f, 0.0f, 0.0f);
    const glm::vec3 v2(0.5f, 0.0f, h);
    const glm::vec3 v3(0.5f, H, h / 3);
    LambertianMaterial groundMat(glm::vec3(1.0f));
    sb.AddQuad(
        glm::vec3(-150.0f, -10.5f, -150.0f),
        glm::vec3(0.0f, -10.5f, 300.0f),
        glm::vec3(300.0f, -10.5f, 0.0f),
        groundMat);

    // -------------------------------------------------------------------------
    for (int i = 0; i < tetraCount; ++i)
    {
        // --- random transform -------------------------------------------------
        glm::vec3 center(distPos(rng), distPos(rng), distPos(rng));
        float      s = distSize(rng);          // side length scale

        // random rotation (quaternion)
        glm::vec3 axis(distAxis(rng), distAxis(rng), distAxis(rng));
        axis = glm::normalize(axis);
        float  angle = distAngle(rng);
        glm::mat3 R = glm::mat3_cast(glm::angleAxis(angle, axis));

        auto W = [&](const glm::vec3& p) -> glm::vec3
            {
                return R * (p * s) + center;
            };

        // --- random material --------------------------------------------------
        Material* mat;
        glm::vec3 tint(distColor(rng), distColor(rng), distColor(rng));

        if (choose(rng) < 0.3f)
        {
            mat = new MetalMaterial(glm::vec3(tint), 0.00f);
        }
        else if (choose(rng) < 0.6f)
        {
            mat = new DialectricMaterial();
        }
        else
        {
            mat = new LambertianMaterial(tint);
        }
        
        // --- four triangular faces -------------------------------------------
        sb.AddTriangle(W(v0), W(v1), W(v2), *mat); // base
        sb.AddTriangle(W(v0), W(v1), W(v3), *mat); // side 1
        sb.AddTriangle(W(v1), W(v2), W(v3), *mat); // side 2
        sb.AddTriangle(W(v2), W(v0), W(v3), *mat); // side 3
    }

    return sb.Build();

}
