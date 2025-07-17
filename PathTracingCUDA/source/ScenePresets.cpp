#include "../include/ScenePresets.h"
#include "../include/SceneBuilder.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <random>

Scene* Scenes::RayTracingInOneWeekend(int seed, Camera*& cam)
{
    cam->center = glm::vec3(13, 2, 3);
    cam->vfov = 20;
    cam->lookAt = glm::vec3(0, 0, 0);
    cam->Init();

    SceneBuilder wb;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);

    //floor
    LambertianMaterial groundMat(glm::vec3(0.8f, 0.8f, 0.8f));
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

Scene* Scenes::KaisScene(int seed, Camera*& cam)
{
    SceneBuilder sb;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);

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

Scene* Scenes::TriangleTestScene(int seed, Camera*& cam )
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

        if (choose(rng) < 1.3f)
        {
            mat = new MetalMaterial(glm::vec3(tint), 0.00f);
        }
        else if (choose(rng) < 0.5f)
        {
            mat = new DialectricMaterial();
        }
        else
        {
            mat = new LambertianMaterial(tint);
        }

        mat = new LambertianMaterial(tint);
        
        // --- four triangular faces -------------------------------------------
        sb.AddTriangle(W(v0), W(v1), W(v2), *mat); // base
        sb.AddTriangle(W(v0), W(v1), W(v3), *mat); // side 1
        sb.AddTriangle(W(v1), W(v2), W(v3), *mat); // side 2
        sb.AddTriangle(W(v2), W(v0), W(v3), *mat); // side 3
    }

    return sb.Build();

}

Scene* Scenes::PlaneTestScene(int seed, Camera*& cam)
{
    SceneBuilder sb;

    float gap = 0.0f;

    (cam)->lookAt = glm::vec3(0, .5, 0);
    (cam)->center = glm::vec3(-2, 0.5, 2);
    cam->vfov = 25;
    cam->Init();

    //floor
    sb.AddQuad(glm::vec3(0,0,0), glm::vec2(10,10), glm::vec4(1,0,0,0), LambertianMaterial(glm::vec3(0.5f, 0.5f, 0.5f)));

    //roof
    sb.AddQuad(glm::vec3(-2.5f - gap, 1, 0), glm::vec2(5, 10), glm::vec4(1, 0, 0, 0), LambertianMaterial(glm::vec3(0.5f, 0.5f, 0.5f)));
    sb.AddQuad(glm::vec3(2.5f + gap, 1, 0), glm::vec2(5, 10), glm::vec4(1, 0, 0, 0), LambertianMaterial(glm::vec3(0.5f, 0.5f, 0.5f)));
    
    //x walls (red)
    sb.AddQuad(glm::vec3(.0f, 0.5f, 5.0), glm::vec2(10, 2), glm::vec4(1, 0, 0, 90), LambertianMaterial(glm::vec3(1.0f, 0.5f, 0.5f)));
    sb.AddQuad(glm::vec3(.0f, 0.5f, -5.0), glm::vec2(10, 2), glm::vec4(1, 0, 0, 90), LambertianMaterial(glm::vec3(1.0f, 0.5f, 0.5f)));

    //z walls (green)
    sb.AddQuad(glm::vec3(-5.0f, 0.5f, .0f), glm::vec2(2, 10), glm::vec4(0, 0, 1, 90), LambertianMaterial(glm::vec3(0.5f, 1.0f, 0.5f)));
    sb.AddQuad(glm::vec3(5.0f, 0.5f, .0f), glm::vec2(2, 10), glm::vec4(0, 0, 1, 90), LambertianMaterial(glm::vec3(0.5f, 1.0f, 0.5f)));

    //spheres
    sb.AddSphere(glm::vec3(0.0f, 0.3f, 0.0f), 0.3f, MetalMaterial(glm::vec3(0.3f, 0.3f, 0.3f), 0.05f));
    sb.AddSphere(glm::vec3(0.0f, 0.1f, 0.5f), 0.1f, DialectricMaterial());
    sb.AddSphere(glm::vec3(1.0f, 0.25f, 0.0f), 0.25f, DialectricMaterial());

    //lights

    for (int i = -5; i <= 5; i++)
    {
        for (int j = -5; j <= 5; j++)
        {
            //sb.AddQuad(glm::vec3((float)i, 1.0f - 0.001f, (float)j), glm::vec2(0.2f), glm::vec4(1, 0, 0, 0), DiffuseLightMaterial(glm::vec3(5.0f)));
        }
    }

    sb.AddQuad(glm::vec3(0.0f, 1.0f - 0.001f, 0.0f), glm::vec2(0.2f), glm::vec4(1, 0, 0, 0), DiffuseLightMaterial(glm::vec3(5.0f)));
    

    return sb.Build();
}

Scene* Scenes::CornellBoxScene(int /*seed*/, Camera*& cam)
{
    // --- Camera setup ---
    cam->center = glm::vec3(0.00f, 278.0f, 800.0f);
    cam->lookAt = glm::vec3(0.0f, 278.0f, 0.0f);
    cam->vfov = 50.0f;
    cam->backgroundColor = glm::vec3(0.0f);
    cam->Init();

    SceneBuilder sb;

    // --- Materials ---
    LambertianMaterial white(glm::vec3(0.73f));
    LambertianMaterial red(glm::vec3(0.65f, 0.05f, 0.05f));
    LambertianMaterial green(glm::vec3(0.12f, 0.45f, 0.15f));
    DiffuseLightMaterial light(glm::vec3(15.0f));

    // --- Cornell box dimensions ---
    const float W = 555.0f;   // box width, height, depth

    // Floor (y = 0)
    //sb.AddQuad(
    //    /*origin*/ glm::vec3(0.0f, 0.0f, 0.0f),
    //    /*u      */ glm::vec3(W, 0.0f, 0.0f),
    //    /*v      */ glm::vec3(0.0f, 0.0f, W),
    //    white
    //);

    sb.AddQuad(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(W), glm::vec4(1, 0, 0, 0), white);


    // Ceiling (y = W)
    //sb.AddQuad(
    //    /*origin*/ glm::vec3(0.0f, W, W),
    //    /*u      */ glm::vec3(W, 0.0f, 0.0f),
    //    /*v      */ glm::vec3(0.0f, 0.0f, -W),
    //    white
    //);

    sb.AddQuad(glm::vec3(0.0f, W, 0.0f), glm::vec2(W), glm::vec4(1, 0, 0, 0), white);


    // Back wall (z = W)
    //sb.AddQuad(
    //    /*origin*/ glm::vec3(0.0f, 0.0f, W),
    //    /*u      */ glm::vec3(W, 0.0f, 0.0f),
    //    /*v      */ glm::vec3(0.0f, W, 0.0f),
    //    white
    //);
    sb.AddQuad(glm::vec3(0.0f, W / 2, -W / 2), glm::vec2(W), glm::vec4(1, 0, 0, 90), white);


    // Left wall (x = 0), red
    //sb.AddQuad(
    //    /*origin*/ glm::vec3(0.0f, 0.0f, 0.0f),
    //    /*u      */ glm::vec3(0.0f, 0.0f, W),
    //    /*v      */ glm::vec3(0.0f, W, 0.0f),
    //    red
    //);
    sb.AddQuad(glm::vec3(-W/ 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), red);

    // Right wall (x = W), green
    //sb.AddQuad(
    //    /*origin*/ glm::vec3(W, 0.0f, W),
    //    /*u      */ glm::vec3(0.0f, 0.0f, -W),
    //    /*v      */ glm::vec3(0.0f, W, 0.0f),
    //    green
    //);

    sb.AddQuad(glm::vec3(W / 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), green);

    // --- Area light on the ceiling ---
    sb.AddQuad(
        glm::vec3(0.0f, W - 0.1f, 0.0f),
        glm::vec2(200.0f),
        glm::vec4(1,0,0,0),  // positive Z
        light
    );

    sb.AddSphere(glm::vec3(110.0f, 100.0f, -60.0f), 100.0f, LambertianMaterial(glm::vec3(0.4f, 0.5f, 1.0f)));
    sb.AddSphere(glm::vec3(-20.0f, 80.0f, 50.0f), 80.0f, MetalMaterial(glm::vec3(1.0f), 0.02f));
    sb.AddSphere(glm::vec3(0.0f - 120.0f, 50.0f, 0.0f - 120.0f), 50.0f, DialectricMaterial());
    
    

    return sb.Build();
}
