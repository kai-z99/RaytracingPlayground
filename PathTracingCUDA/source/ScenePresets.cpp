#include "../include/ScenePresets.h"
#include "../include/SceneBuilder.h"

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
