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
                DielectricMaterial m(glm::vec3(1.0f), 1.5f);
                wb.AddSphere(center, 0.2f, m);
            }
        }
    }

    //3 large spheres
    DielectricMaterial glass(glm::vec3(1.0f), 1.5f);
    wb.AddSphere(glm::vec3(0, 1, 0), 1.0f, glass);

    LambertianMaterial lam(glm::vec3(0.4f, 0.2f, 0.1f));
    wb.AddSphere(glm::vec3(-4, 1, 0), 1.0f, lam);

    MetalMaterial metal(glm::vec3(0.7f, 0.6f, 0.5f), 0.05f);
    wb.AddSphere(glm::vec3(4, 1, 0), 1.0f, metal);

    return wb.Build();
}

Scene* Scenes::TriangleTestScene(int seed, Camera*& cam )
{
    (cam)->lookAt = glm::vec3(0, 0, 0);
    (cam)->center = glm::vec3(0, 30, 70);
    cam->vfov = 90;
    cam->Init();

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
        else if (choose(rng) < 0.5f)
        {
            mat = new DielectricMaterial(glm::vec3(1.0f));
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

Scene* Scenes::PlaneTestScene(int seed, Camera*& cam)
{
    SceneBuilder sb;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);
    std::uniform_real_distribution<float> U2(-1.0f, 1.0f);

    float gap = 0.0f;

    (cam)->lookAt = glm::vec3(0, .5, 0);
    (cam)->center = glm::vec3(-2, 0.5, 2);
    cam->vfov = 50;
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
    sb.AddSphere(glm::vec3(0.0f, 0.1f, 0.5f), 0.1f, DielectricMaterial(glm::vec3(1.0f)));
    sb.AddSphere(glm::vec3(1.0f, 0.25f, 0.0f), 0.25f, DielectricMaterial(glm::vec3(1.0f)));

    for (int i = 0; i < 200; i++)
    {
        float r = 0.05f + U(rng) * 0.12f;
        sb.AddSphere(glm::vec3(U2(rng) * 4, r, U2(rng) * 4), r, LambertianMaterial(glm::vec3(U(rng), U(rng), U(rng))));
    }
    

    //lights

    for (int i = -5; i <= 5; i++)
    {
        for (int j = -5; j <= 5; j++)
        {
            sb.AddQuad(glm::vec3((float)i, 1.0f - 0.001f, (float)j), glm::vec2(0.2f), glm::vec4(1, 0, 0, 0), DiffuseLightMaterial(glm::vec3(5.0f)));
        }
    }

    //sb.AddQuad(glm::vec3(0.0f, 1.0f - 0.001f, 0.0f), glm::vec2(0.2f), glm::vec4(1, 0, 0, 0), DiffuseLightMaterial(glm::vec3(5.0f)));
    

    return sb.Build();
}

//Rendertime for 1000spp 12 bounce: 535.266 secs
//With model: 1757.1 secs, 15bounce, 1000sp, MODEL LOADED. TRIANGLES: 871306
//Dragon 999 seconds, 10'000sp, 1020x1080, 15 bounce
//dragon dialectric :  1571.88 secs

//800x600, 100spp, 15 bounce, dragon
//Lambertian:  35.7787 secs
//Metal: 36.9474 secs
//dialectric: 50.1525 secs

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
    MetalMaterial metal(glm::vec3(1.0f), 0.0f);
    DielectricMaterial die(glm::vec3(1.0f), 1.5f, 0.2f);
    
    SubsurfaceMaterial sm = SubsurfaceMaterial(
        glm::vec3(1.0f, 1.0f, 1.0f), 
        glm::vec3(0.0f, 0.659f, 0.42f),
        1.f,
        10.0f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.2f

        //jade: 0.0f, 0.659f, 0.42f
    );

    PBRMaterial m(glm::vec3(0.4f, 0.0f, 0.0f), 0.0f, 0.1f);

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

   
    //sb.AddSphere(glm::vec3(110.0f, 100.0f, -60.0f), 100.0f, LambertianMaterial(glm::vec3(0.4f, 0.5f, 1.0f)));
    //sb.AddSphere(glm::vec3(-20.0f, 80.0f, 50.0f), 80.0f, MetalMaterial(glm::vec3(1.0f), 0.02f));
    //sb.AddSphere(glm::vec3(0.0f - 120.0f, 50.0f, 0.0f - 120.0f), 50.0f, DielectricMaterial());
    //sb.AddTriangle(glm::vec3(-W/2 + 30.0f, 50.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 30.0f, 225.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 210.0f, 90.0f, -W/2 + 10.0f), MetalMaterial(glm::vec3(1.0f), 0.02f));
    //

    std::string objName = "dragon.obj";
    float scale = 350.00f;
    float yExtent = 0.7f;
    float rotDeg = 90.0f;

    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale, 0));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f)); //2
    //M = glm::rotate(M, glm::radians(-45.0f), glm::vec3(1.0f, 0.0f, 0.0f)); //1
    M = glm::scale(M, glm::vec3(scale));

    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, PBRMaterial(glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.225f));
    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, DielectricMaterial());

    //LambertianMaterial m = LambertianMaterial(glm::vec3(0.7f, 0.7f, 0.7f));
    //SubsurfaceMaterial sm = SubsurfaceMaterial(
    //    glm::vec3(0.7f, 0.7f, 0.7f),
    //    glm::vec3(0.7f, 0.7f, 0.7f), //
    //    1.0f,
    //    5.6f,
    //    1.5f,
    //    1.0f
    //);

    

    //glm::vec3(1.0f, 0.4f, 0.4f),
    //glm::vec3(0.4f, 0.4f, 1.0f), 
    
    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, die);

    //sb.AddSphere(glm::vec3(0, 200, 0), 150, die);
    //sb.AddBox(glm::vec3(0, 200, 0), glm::vec3(150), glm::vec4(1,1,0,30), die);

    M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(-110, (yExtent / 2.0f) * scale, 0));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f)); //2
   // M = glm::rotate(M, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); //1
    M = glm::scale(M, glm::vec3(scale));

    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, m);

    //sb.AddSphere(glm::vec3(0.0f, 100.0f, 0.0f), 100.0f, sm);

    //sb.AddBox(glm::vec3(0.0f, 268.0f, 0.0f), glm::vec3(300.0f, 30.0f, 300.0f), glm::vec4(0,1,1,45), sm);
    //sb.AddTriangle({ 0.0f, 250, 0.0f }, { 130.0f, 150, 0 }, {0,0, 200}, sm);

    return sb.Build();
}

Scene* Scenes::SlabScene(int seed, Camera*& cam)
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

    SubsurfaceMaterial sm = SubsurfaceMaterial(
        glm::vec3(0.6f, 1.00f, 0.6f), //green albedo
        glm::vec3(1.0f, 0.788f, 0.667f), //red sss == yellow
        1.0f,
        50.0f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.5f
    );

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
    sb.AddQuad(glm::vec3(-W / 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), red);

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
        glm::vec4(1, 0, 0, 0),  // positive Z
        light
    );


    //sb.AddSphere(glm::vec3(110.0f, 100.0f, -60.0f), 100.0f, LambertianMaterial(glm::vec3(0.4f, 0.5f, 1.0f)));
    //sb.AddSphere(glm::vec3(-20.0f, 80.0f, 50.0f), 80.0f, MetalMaterial(glm::vec3(1.0f), 0.02f));
    //sb.AddSphere(glm::vec3(0.0f - 120.0f, 50.0f, 0.0f - 120.0f), 50.0f, DielectricMaterial());
    //sb.AddTriangle(glm::vec3(-W/2 + 30.0f, 50.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 30.0f, 225.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 210.0f, 90.0f, -W/2 + 10.0f), MetalMaterial(glm::vec3(1.0f), 0.02f));
    //

    std::string objName = "dragon.obj";
    float scale = 350.00f;
    float yExtent = 0.700f;
    float rotDeg = 90.0f;

    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale, 0));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f)); //2
    //M = glm::rotate(M, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); //1
    M = glm::scale(M, glm::vec3(scale));

    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, PBRMaterial(glm::vec3(1.0f, 1.0f, 1.0f), 1.0f, 0.225f));
    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, DielectricMaterial());

    LambertianMaterial m = LambertianMaterial(glm::vec3(0.7f, 0.7f, 0.7f));
    //SubsurfaceMaterial sm = SubsurfaceMaterial(
    //    glm::vec3(0.7f, 0.7f, 0.7f),
    //    glm::vec3(0.7f, 0.7f, 0.7f), //
    //    1.0f,
    //    5.6f,
    //    1.5f,
    //    1.0f
    //);



    //glm::vec3(1.0f, 0.4f, 0.4f),
    //glm::vec3(0.4f, 0.4f, 1.0f), 

    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, sm);

    M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(-110, (yExtent / 2.0f) * scale, 0));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f)); //2
    // M = glm::rotate(M, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); //1
    M = glm::scale(M, glm::vec3(scale));

    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, m);

    //sb.AddSphere(glm::vec3(0.0f, 100.0f, 0.0f), 100.0f, sm);
    sb.AddBox(glm::vec3(0.0f, 268.0f, 0.0f), glm::vec3(300.0f, 30.0f, 300.0f), glm::vec4(0, 1, 1, 45), sm);

    return sb.Build();
}

Scene* Scenes::SkyScene(int seed, Camera*& cam)
{
    cam->center = glm::vec3(0, 600, 1300);
    cam->vfov = 10;
    cam->lookAt = glm::vec3(0, 50, 0); //xyz: 50 s: 100
    cam->backgroundColor = glm::vec3(0.70, 0.80, 1.00) * 0.0f; //0.5
    cam->Init();

    SceneBuilder wb;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);

    LambertianMaterial groundMat(glm::vec3(0.7f, 0.7f, 0.7f));
    wb.AddQuad(
        glm::vec3(-3000.0f, 0.0f, -3000.0f),
        glm::vec3(0.0f, 0.0f, 6000.0f),
        glm::vec3(6000.0f, 0.0f, 0.0f),
        groundMat);

    DielectricMaterial glass(glm::vec3(1.0f), 1.5f);

    DiffuseLightMaterial light(glm::vec3(7.0f));

    wb.AddQuad(glm::vec3(0.0f, 300.0f, -0.0f), glm::vec2(250.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), light);

    SubsurfaceMaterial sm = SubsurfaceMaterial(
        glm::vec3(1.f, 1.f, 1.f),
        glm::vec3(1.0, 0.365, 0.216), //xyz:  s: 0.16, 0.837, 0.25
        1.0f,
        0.10f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.2f //xyz: 0.2 s: 0.25
    );

    PBRMaterial pm = PBRMaterial
    (
        { 0.9, 0.365, 0.216 },
        0.0f,
        0.0f
    );

    std::string objName = "xyzrgb_dragon.obj";
    float scale = 350.00f;
    float yExtent = 0.55f; //xyz: 0.55 s: 0.705
    float rotDeg = -60.0f - 70; //xyz: -60 s: -50

    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale - 40, 0));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));

    wb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, glass);


    return wb.Build();
}

Scene* Scenes::PassthroughScene(int seed, Camera*& cam)
{
    cam->center = glm::vec3(0, 600, 1300);
    cam->vfov = 10;
    cam->lookAt = glm::vec3(0, 50, 0); //xyz: 50 s: 100
    cam->backgroundColor = glm::vec3(0.70, 0.80, 1.00) * 0.0f;
    cam->Init();

    SceneBuilder wb;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);

    LambertianMaterial groundMat(glm::vec3(0.7f, 0.7f, 0.7f));
    wb.AddQuad(
        glm::vec3(-3000.0f, 0.0f, -3000.0f),
        glm::vec3(0.0f, 0.0f, 6000.0f),
        glm::vec3(6000.0f, 0.0f, 0.0f),
        groundMat);

    DiffuseLightMaterial light(glm::vec3(7.0f));

    wb.AddQuad(glm::vec3(0.0f, 150.0f, -200.0f), glm::vec2(250), glm::vec4(1.0f, 0.0f, 0.0f, -70.0f), light);

    SubsurfaceMaterial sm = SubsurfaceMaterial(
        glm::vec3(1.f, 1.f, 1.f),
        glm::vec3(1.0, 0.365, 0.216), //xyz:  s: 0.16, 0.837, 0.25
        1.0f,
        0.1f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.2f //xyz: 0.2 s: 0.25
    );

    PBRMaterial pm = PBRMaterial
    (
        { 0.9, 0.365, 0.216 },
        0.0f,
        0.0f
    );

    std::string objName = "xyzrgb_dragon.obj";
    float scale = 350.00f;
    float yExtent = 0.55f; //xyz: 0.55 s: 0.705
    float rotDeg = -60.0f - 70; //xyz: -60 s: -50

    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale - 40, 0));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));

    wb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, sm);


    return wb.Build();
}

Scene* Scenes::DragonScene(int seed, Camera*& cam)
{
    cam->center = glm::vec3(0, 600, 1300);
    cam->vfov = 17.5f;
    cam->lookAt = glm::vec3(0, 175, 0); //xyz: 50 s: 100
    cam->backgroundColor = glm::vec3(0.70, 0.80, 1.00) * 0.0f;
    cam->Init();

    SceneBuilder wb;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);

    LambertianMaterial groundMat(glm::vec3(0.7f, 0.7f, 0.7f));
    wb.AddQuad(
        glm::vec3(-3000.0f, 0.0f, -3000.0f),
        glm::vec3(0.0f, 0.0f, 6000.0f),
        glm::vec3(6000.0f, 0.0f, 0.0f),
        groundMat);

    DiffuseLightMaterial light(glm::vec3(50.0f));

    //wb.AddQuad(glm::vec3(0.0f, 150.0f, -170.0f), glm::vec2(250.0f), glm::vec4(1.0f, 0.0f, 0.0f, -70.0f), light);
    wb.AddSphere(glm::vec3(0.0f, 100.0f, -150.0f), 35.0f, light);

    SubsurfaceMaterial dragonMat = SubsurfaceMaterial(
        glm::vec3(1.f, 1.f, 1.f),
        glm::vec3(0.0f, 0.659f, 0.42f), //xyz:  s: 0.16, 0.837, 0.25
        1.0f,
        10.00f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.2f //xyz: 0.2 s: 0.25
    );

    SubsurfaceMaterial lucyMat = SubsurfaceMaterial(
        glm::vec3(1.f, 1.f, 1.f),
        glm::vec3(1.0f, 0.25f, 0.35f), //xyz:  s: 0.16, 0.837, 0.25
        1.0f,
        3.50f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.1f //xyz: 0.2 s: 0.25
    );

    SubsurfaceMaterial buddhaMat = SubsurfaceMaterial(
        glm::vec3(1.f, 1.f, 1.f),
        glm::vec3(0.2f, 0.259f, 0.82f), //xyz:  s: 0.16, 0.837, 0.25
        1.0f,
        1.00f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.5f //xyz: 0.2 s: 0.25
    );

    std::string objName = "dragon.obj";
    float scale = 350.00f;
    float yExtent = 0.705f;
    float rotDeg = 90; //xyz: -60 s: -50

    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale, 0));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));

    wb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, dragonMat);


    objName = "lucy.obj";
    scale = 350.00f;
    yExtent = 1.0f;
    rotDeg = 180 - 30; 

    M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(200, (yExtent / 2.0f) * scale, -100));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f)); //2
    M = glm::rotate(M, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); //1
    M = glm::scale(M, glm::vec3(scale));

    wb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, lucyMat);

    objName = "buddha.obj";
    scale = 350.00f;
    yExtent = 1.0f;
    rotDeg = 180 + 45;

    M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(-200, (yExtent / 2.0f) * scale, -100));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));

    wb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, buddhaMat);


    return wb.Build();
}

Scene* Scenes::MetalHeadScene(int seed, Camera*& cam)
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
    SubsurfaceMaterial skin = SubsurfaceMaterial(
        glm::vec3(0.6f, 1.00f, 0.6f), //green albedo
        glm::vec3(1.0f, 0.788f, 0.667f), //red sss == yellow
        1.0f,
        20.5f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.5f
    );

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
    sb.AddQuad(glm::vec3(-W / 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), red);

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
        glm::vec4(1, 0, 0, 0),  // positive Z
        light
    );


    //sb.AddSphere(glm::vec3(110.0f, 100.0f, -60.0f), 100.0f, LambertianMaterial(glm::vec3(0.4f, 0.5f, 1.0f)));
    //sb.AddSphere(glm::vec3(-20.0f, 80.0f, 50.0f), 80.0f, MetalMaterial(glm::vec3(1.0f), 0.02f));
    //sb.AddSphere(glm::vec3(0.0f - 120.0f, 50.0f, 0.0f - 120.0f), 50.0f, DielectricMaterial());
    //sb.AddTriangle(glm::vec3(-W/2 + 30.0f, 50.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 30.0f, 225.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 210.0f, 90.0f, -W/2 + 10.0f), MetalMaterial(glm::vec3(1.0f), 0.02f));
    //

    std::string objName = "obj_free_male_head.OBJ";
    float scale = 250.00f;
    float yExtent = 1.0;
    float rotDeg = 30.0f;
    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale * 1.3f + 100, 50));
    M = glm::rotate(M, -glm::radians(rotDeg), glm::vec3(1.0f, 0.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale * 1.3f));
    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, skin);

    M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(-170, (yExtent / 2.0f) * scale, -30));
    M = glm::rotate(M, -glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    //M = glm::rotate(M, -glm::radians(rotDeg), glm::vec3(1.0f, 0.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));
    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, MetalMaterial(glm::vec3(0.5f, 1.0f, 0.5f), 0.25f));

    M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(170, (yExtent / 2.0f) * scale, -30));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    //M = glm::rotate(M, -glm::radians(rotDeg), glm::vec3(1.0f, 0.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));
    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, MetalMaterial(glm::vec3(1.0f, 0.5f, 0.5f), 0.25f));
    

    return sb.Build();
}

Scene* Scenes::HeadScene(int seed, Camera*& cam)
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
    SubsurfaceMaterial skin = SubsurfaceMaterial(
        glm::vec3(0.6f, 1.00f, 0.6f), //green albedo
        glm::vec3(0.906f, 0.737f, 0.569f), 
        1.0f,
        0.3f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.5f
    );
    LambertianMaterial skinDiffuse(glm::vec3(0.906f, 0.737f, 0.569f));

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
    sb.AddQuad(glm::vec3(-W / 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), red);

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
        glm::vec4(1, 0, 0, 0),  // positive Z
        light
    );


    //sb.AddSphere(glm::vec3(110.0f, 100.0f, -60.0f), 100.0f, LambertianMaterial(glm::vec3(0.4f, 0.5f, 1.0f)));
    //sb.AddSphere(glm::vec3(-20.0f, 80.0f, 50.0f), 80.0f, MetalMaterial(glm::vec3(1.0f), 0.02f));
    //sb.AddSphere(glm::vec3(0.0f - 120.0f, 50.0f, 0.0f - 120.0f), 50.0f, DielectricMaterial());
    //sb.AddTriangle(glm::vec3(-W/2 + 30.0f, 50.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 30.0f, 225.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 210.0f, 90.0f, -W/2 + 10.0f), MetalMaterial(glm::vec3(1.0f), 0.02f));
    //

    std::string objName = "obj_free_male_head.OBJ";
    float scale = 300.00f;
    float yExtent = 1.0;
    float rotDeg = 30.0f;
    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale + 100, 50));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    M = glm::rotate(M, -glm::radians(rotDeg), glm::vec3(1.0f, 0.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));
    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, skinDiffuse);

    return sb.Build();
}

Scene* Scenes::HandScene(int seed, Camera*& cam)
{
    // --- Camera setup ---
    cam->center = glm::vec3(0.00f, 278.0f, 800.0f);
    cam->lookAt = glm::vec3(0.0f, 278.0f, 0.0f);
    cam->vfov = 23.0f;
    cam->backgroundColor = glm::vec3(0.0f);
    cam->Init();

    SceneBuilder sb;

    // --- Materials ---
    LambertianMaterial white(glm::vec3(0.73f));
    LambertianMaterial red(glm::vec3(0.65f, 0.05f, 0.05f));
    LambertianMaterial green(glm::vec3(0.12f, 0.45f, 0.15f));
    DiffuseLightMaterial light(glm::vec3(8.0f));
    SubsurfaceMaterial skin = SubsurfaceMaterial(
        glm::vec3(0.6f, 1.00f, 0.6f), //green albedo
        glm::vec3(1.0f, 0.788f, 0.667f),
        1.0f,
        3.0f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.5f
    );
    LambertianMaterial skinLambert(glm::vec3(1.0f, 0.788f, 0.667f));
    DielectricMaterial glass(glm::vec3(1.0f), 1.5f);


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
    sb.AddQuad(glm::vec3(-W / 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), red);

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
        glm::vec4(1, 0, 0, 0),  // positive Z
        light
    );


    //sb.AddSphere(glm::vec3(110.0f, 100.0f, -60.0f), 100.0f, LambertianMaterial(glm::vec3(0.4f, 0.5f, 1.0f)));
    //sb.AddSphere(glm::vec3(-20.0f, 80.0f, 50.0f), 80.0f, MetalMaterial(glm::vec3(1.0f), 0.02f));
    //sb.AddSphere(glm::vec3(0.0f - 120.0f, 50.0f, 0.0f - 120.0f), 50.0f, DielectricMaterial());
    //sb.AddTriangle(glm::vec3(-W/2 + 30.0f, 50.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 30.0f, 225.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 210.0f, 90.0f, -W/2 + 10.0f), MetalMaterial(glm::vec3(1.0f), 0.02f));
    //

    std::string objName = "hand.OBJ";
    float scale = 300.00f;
    float yExtent = 1.0;
    float rotDeg = -115.0f;
    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale + 150, 50));
    M = glm::rotate(M, -glm::radians(15.0f), glm::vec3(0.0f, 1.0f, 0.0f)); //1
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(1.0f, 0.0f, 0.0f)); //2
    
    M = glm::scale(M, glm::vec3(scale));
    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, glass);

    return sb.Build();
}

Scene* Scenes::ModelTest(int seed, Camera*& cam)
{
    cam->lookAt = { 0,0,0 };
    cam->center = {0, 1, 3};
    cam->vfov = 20;
    cam->Init();

    SceneBuilder sb;

    glm::mat4 M(1.0f);
    M = glm::rotate(glm::mat4(1.0f), glm::radians(80.0f), glm::vec3(0.0f, 1.0f, 0.0f));


    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/dragon.obj", M, DielectricMaterial());
    sb.AddQuad(glm::vec3(0.0f, -0.70498f / 2.0f, 0.0f), glm::vec2(5.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), LambertianMaterial(glm::vec3(0.7f)));


    return sb.Build();
}

//assorted mat: 1756.86 secs
//assorted diffuse-metal combos: 1503.81 secs
Scene* Scenes::StatueScene(int seed, Camera*& cam)
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
    sb.AddQuad(glm::vec3(-W / 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), red);

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
        glm::vec4(1, 0, 0, 0),  // positive Z
        light
    );


    //sb.AddSphere(glm::vec3(110.0f, 100.0f, -60.0f), 100.0f, LambertianMaterial(glm::vec3(0.4f, 0.5f, 1.0f)));
    //sb.AddSphere(glm::vec3(-20.0f, 80.0f, 50.0f), 80.0f, MetalMaterial(glm::vec3(1.0f), 0.02f));
    //sb.AddSphere(glm::vec3(0.0f - 120.0f, 50.0f, 0.0f - 120.0f), 50.0f, DielectricMaterial());
    //sb.AddTriangle(glm::vec3(-W/2 + 30.0f, 50.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 30.0f, 225.0f, -W/2 + 30.0f), glm::vec3(-W/2 + 210.0f, 90.0f, -W/2 + 10.0f), MetalMaterial(glm::vec3(1.0f), 0.02f));
    //0.816, 0.573, 0.910.3f

    std::string objName = "erato.obj";
    float scale = 450.00f;
    float yExtent = 1;
    float rotDeg = 90.0f;

    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(-150, (yExtent / 2.0f) * scale, -150));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));
    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, PBRMaterial(glm::vec3(0.816, 0.573, 0.91), 0.5f, 0.225f));


    objName = "buddha.obj";

    rotDeg = 135.0f;
    scale = 300.00f;

    M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(150, (yExtent / 2.0f) * scale, -75));
    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    M = glm::scale(M, glm::vec3(scale));
    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, PBRMaterial(glm::vec3(0.502, 0.82, 0.698), 0.8f, 0.225f));

    objName = "lucy.obj";
    rotDeg = 180.0f;
    scale = 200.00f;

    M = glm::mat4(1.0f);
    M = glm::translate(M, glm::vec3(0, (yExtent / 2.0f) * scale, 0));

    M = glm::rotate(M, glm::radians(rotDeg), glm::vec3(0.0f, 1.0f, 0.0f)); //2
    M = glm::rotate(M, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); //1

    M = glm::scale(M, glm::vec3(scale));

    sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/" + objName, M, PBRMaterial(glm::vec3(1, 0.667, 0.4), 1.0f, 0.225f));



    return sb.Build();
}

Scene* Scenes::PBRTest(int seed, Camera*& cam)
{
    cam->center = {0, 1, 2};
    cam->lookAt = { 0, 1, 0 };
    cam->vfov = 80;
    cam->backgroundColor = { 0,0,0 };
    cam->Init();
    SceneBuilder sb;

    float e = 1e-6f;
    sb.AddQuad({ 0,0,0 }, { 6,6 }, { 1,1,1,0 }, MetalMaterial({ 0.3,0.3,0.3 }, 1.0));
    sb.AddQuad({ -3,0,0 }, { 6,6 }, { 0,0,1,90 }, MetalMaterial({ 0.8,0.3,0.3 },  1.0));
    sb.AddQuad({ 3,0,0 }, { 6,6 }, { 0,0,1,90 }, MetalMaterial({ 0.8,0.3,0.3 },  1.0));
    sb.AddQuad({ 0,0,-3 }, { 6,6 }, { 1,0,0,90 }, MetalMaterial({ 0.0,0.8,0.3 }, 1.0));
    sb.AddQuad({ 0,3 + e,0 }, { 6,6 }, { 1,1,1,0 }, MetalMaterial({ 0.3,0.3,0.3 }, 1.0));

    float r = 0.35f;

    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(-2, r, 0));
    M = glm::rotate(M, glm::radians(90.0f), glm::vec3(0, 1, 0));

    //
    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/dragon.obj", M, PBRMaterial({ 1.0,1.0,1.0 }, 0.0f, 0.5f));

    //M = glm::mat4(1.0f);
    //M = glm::translate(M, glm::vec3(-1, r, 0));
    //M = glm::rotate(M, glm::radians(90.0f), glm::vec3(0, 1, 0));

    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/dragon.obj", M, PBRMaterial({ 1.0,1.0,1.0 }, 0.2f, 0.5f));
    //M = glm::mat4(1.0f);
    //M = glm::translate(M, glm::vec3(0, r, 0));
    //M = glm::rotate(M, glm::radians(90.0f), glm::vec3(0, 1, 0));

    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/dragon.obj", M, PBRMaterial({ 1.0,1.0,1.0 }, 0.5f, 0.5f));
    //M = glm::mat4(1.0f);
    //M = glm::translate(M, glm::vec3(1, r, 0));
    //M = glm::rotate(M, glm::radians(90.0f), glm::vec3(0, 1, 0));
    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/dragon.obj", M, PBRMaterial({ 1.0,1.0,1.0 }, 0.7f, 0.5f));

    //M = glm::mat4(1.0f);
    //M = glm::translate(M, glm::vec3(2, r, 0));
    //M = glm::rotate(M, glm::radians(90.0f), glm::vec3(0, 1, 0));
    //sb.AddModel("C:/repos/C++/RayTracingPlayground/PathTracingCUDA/resources/models/dragon.obj", M, PBRMaterial({ 1.0,1.0,1.0 }, 1.0f, 0.5f));
    //
    
    sb.AddSphere({ -2, r,0 }, r, MetalMaterial({ 1.0,0.8,0.8 }, 0.0f));
    sb.AddSphere({ -1, r,0 }, r, MetalMaterial({ 1.0,0.8,0.8 }, 0.2f));
    sb.AddSphere({ -0, r,0 }, r, MetalMaterial({ 1.0,0.8,0.8 }, 0.5f));
    sb.AddSphere({  1, r,0 }, r, MetalMaterial({ 1.0,0.8,0.8 }, 0.7f));
    sb.AddSphere({  2, r,0 }, r, MetalMaterial({ 1.0,0.8,0.8 }, 1.0f));
    
    sb.AddQuad(glm::vec3(-2, 3, 0), {1,1},  { 1, 1, 1, 0 }, DiffuseLightMaterial(glm::vec3(10.0f)));
    sb.AddQuad(glm::vec3(-1, 3, 0), { 1,1 }, { 1, 1, 1, 0 }, DiffuseLightMaterial(glm::vec3(10.0f)));
    sb.AddQuad(glm::vec3(0, 3, 0), { 1,1 }, { 1, 1, 1, 0 }, DiffuseLightMaterial(glm::vec3(10.0f)));
    sb.AddQuad(glm::vec3(1, 3, 0), { 1,1 }, { 1, 1, 1, 0 }, DiffuseLightMaterial(glm::vec3(10.0f)));
    sb.AddQuad(glm::vec3(2, 3, 0), { 1,1 }, { 1, 1, 1, 0 }, DiffuseLightMaterial(glm::vec3(10.0f)));

    return sb.Build();
}

Scene* Scenes::CornellBoxOGScene(int seed, Camera*& cam)
{
    // ---------------------------------------------------------------------
    // Camera
    // ---------------------------------------------------------------------
    cam->center = glm::vec3(0.0f, 278.0f, 800.0f);
    cam->lookAt = glm::vec3(0.0f, 278.0f, 0.0f);
    cam->vfov = 55.0f;
    cam->backgroundColor = glm::vec3(0.0f);
    cam->Init();

    // ---------------------------------------------------------------------
    SceneBuilder sb;

    // Materials
    LambertianMaterial white(glm::vec3(0.73f));
    LambertianMaterial red(glm::vec3(0.65f, 0.05f, 0.05f));
    LambertianMaterial green(glm::vec3(0.12f, 0.45f, 0.15f));
    PBRMaterial metal(glm::vec3(1.0f), 1.0f, 0.0f);
    SubsurfaceMaterial sm = SubsurfaceMaterial(
        glm::vec3(0.6f, 1.00f, 0.6f), //green albedo
        glm::vec3(1.0f, 0.3f, 0.3f), //red sss == yellow
        1.0f,
        5.0f,
        1.0f,
        1.0f,
        1.5f,
        1.0f,
        0.5f
    );

    DiffuseLightMaterial light(glm::vec3(15.0f));

    // Cornell box size
    const float W = 555.0f;                 // width  = height = depth

    // ---------------------------------------------------------------------
    // 5 walls (front is left open for the camera)
    // ---------------------------------------------------------------------
    sb.AddQuad(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec2(W), glm::vec4(1, 0, 0, 0), white); // floor
    sb.AddQuad(glm::vec3(0.0f, W, 0.0f), glm::vec2(W), glm::vec4(1, 0, 0, 0), white); // ceiling
    sb.AddQuad(glm::vec3(0.0f, W / 2, -W / 2), glm::vec2(W), glm::vec4(1, 0, 0, 90), white); // back
    sb.AddQuad(glm::vec3(-W / 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), red);   // left
    sb.AddQuad(glm::vec3(W / 2, W / 2, 0.0f), glm::vec2(W), glm::vec4(0, 0, 1, 90), green); // right

    // ---------------------------------------------------------------------
    // ---------------------------------------------------------------------
    sb.AddQuad(glm::vec3(0.0f, W - 0.1f, 0.0f),
        glm::vec2(200.0f),
        glm::vec4(1, 0, 0, 0),
        light);

    // ---------------------------------------------------------------------
    // ---------------------------------------------------------------------
    sb.AddBox(glm::vec3(70.0f, 82.5f, 100.0f),          // centre
        glm::vec3(165.0f, 165.0f, 165.0f),          // size
        glm::vec4(0, 1, 0, -18.0f),                 // rotation
        sm);

    // ---------------------------------------------------------------------
    // ---------------------------------------------------------------------
    sb.AddBox(glm::vec3(-100.0f, 165, -100.0f),         // centre
        glm::vec3(165.0f, 330.0f, 165.0f),          // size
        glm::vec4(0, 1, 0, 19.0f),                 
        metal);
    
    return sb.Build();
}