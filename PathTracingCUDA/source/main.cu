#include "../include/SetupHelper.h"
#include "../include/Shader.h"

#include "../include/Generic.h"
#include "../include/Camera.h"
#include "../include/Hittable.h"
#include "../include/HittableList.h"
#include "../include/Sphere.h"
#include "../include/BVH.h"

#include "../include/CudaHelper.h"
#include "device_launch_parameters.h"
#include <curand_kernel.h>

#include <chrono>
#include <random>

//todo: camera POD
//todo: make screen w/h a part of config

struct Config
{
    int samplesPerPixel;
    int maxBounceDepth;
};

Config MakeConfig()
{
    Config c;
    c.samplesPerPixel = 15;
    c.maxBounceDepth = 15;

    std::cout << "CUDA VERSION" << '\n';
    std::cout << "RESOLUTION: " << std::to_string(SCREEN_WIDTH) << "x" << std::to_string(SCREEN_HEIGHT) << "px\n";
    std::cout << "SAMPLES PER PIXEL: " << c.samplesPerPixel << '\n';
    std::cout << "MAX BOUNCE DEPTH: " << c.maxBounceDepth << '\n';

    return c;
}

__global__ void BuildCameraKernel(Camera** outCamera, unsigned char* pixels, int samplesPerPixel, int maxDepth)
{
    if (threadIdx.x == 0 && threadIdx.y == 0)
    {
        *outCamera = new Camera();
        (*outCamera)->samplesPerPixel = samplesPerPixel;
        (*outCamera)->maxRayDepth = maxDepth;
        (*outCamera)->SetPixelBuffer(pixels);
    }
}

__global__ void DestroyCameraKernel(Camera** camera)
{
    if (threadIdx.x == 0 && blockIdx.x == 0)
    {
        delete* camera;
    }
}

__global__ void RenderKernel(Camera** camera, Scene scene, curandState* pixelRandStates)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= SCREEN_WIDTH || j >= SCREEN_HEIGHT) return;

    int pixelIndex = j * SCREEN_WIDTH + i;
    curandState randState = pixelRandStates[pixelIndex];

    (*camera)->RenderPixel(randState, scene, i, j);

    //update the state after using it
    pixelRandStates[pixelIndex] = randState;

    if (threadIdx.x == 0 && threadIdx.y == 0 && blockIdx.x == 0)
    {
        float pct = 100.f * (blockIdx.y + 1) / gridDim.y;
        printf("Progress: %.1f%% (row %d of %d)\n", pct, blockIdx.y + 1, gridDim.y);
    }
}

__global__ void InitPixelRandStatesKernel(curandState* randStates, int seed)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i >= SCREEN_WIDTH || j >= SCREEN_HEIGHT) return;

    int idx = j * SCREEN_WIDTH + i;

    curand_init(seed + idx, 0, 0, &randStates[idx]);
}

void InitCUDA()
{
    cudaDeviceSetLimit(cudaLimitStackSize, 16384); //for recursion...
    size_t heap = 64 * 1024 * 1024;   //64mb fir device news
    cudaDeviceSetLimit(cudaLimitMallocHeapSize, heap); //for device new in BuildWorldKernel
}

void SetupRandomPixelStates(curandState*& dPixelRandomStates, int seed)
{
    std::cout << "CREATING RANDOM STATES...\n";
    cudaMalloc(&dPixelRandomStates, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(curandState)); //one for each pixel
    dim3 block(16, 16);
    dim3 grid(CeilDiv(SCREEN_WIDTH, block.x), CeilDiv(SCREEN_HEIGHT, block.y));
    InitPixelRandStatesKernel<<<grid, block>>>(dPixelRandomStates, seed);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());
    std::cout << "CREATED RANDOM STATES\n";

}

void BuildCamera(Camera**& camera, unsigned char* pixelBuffer, const Config& config)
{
    checkCudaErrors(cudaMalloc(&camera, sizeof(Camera*)));
    BuildCameraKernel<<<1, 1>>>(camera, pixelBuffer, config.samplesPerPixel, config.maxBounceDepth);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());
}


void BuildSceneCPU(int seed, Scene*& uScene)
{
    std::cout << "BUILDING WORLD...\n";

    //sphere
    std::vector<glm::vec4> hcenterRadii;
    std::vector<int> hSphereMatIDs; 

    //quad
    std::vector<glm::vec3> hQ;
    std::vector<glm::vec3> hU;
    std::vector<glm::vec3> hV;
    std::vector<int> hQuadMatIDs;
    
    //materials
    std::vector<MaterialData> hMats;

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> U(0.0f, 1.0f);

    auto pushMat = [&](const MaterialData& m)->int 
    {
    hMats.push_back(m);
    return int(hMats.size()) - 1;
    };

    //int groundID = pushMat({glm::vec3(0.5f, 0.5f, 0.5f), 0, 1, MAT_LAMBERTIAN});
    //hcenterRadii.push_back(glm::vec4(0, -1000.0f, 0.0f, 1000.0f));
    //hSphereMatIDs.push_back(groundID);

    //random balls
    for (int a = -11; a < 11; ++a)
    {
        for (int b = -11; b < 11; ++b)
        {
            float choose = U(rng);
            glm::vec3 center = glm::vec3(a + 0.9f * U(rng), 0.2f, b + 0.9f * U(rng));
            if (glm::length(center - glm::vec3(4, 0.2f, 0)) < .9f) continue;

            MaterialData m{};
            if (choose < .8f) 
            {                      // diffuse
                m = { glm::vec3(U(rng) * U(rng),
                                 U(rng) * U(rng),
                                 U(rng) * U(rng)),0,1,MAT_LAMBERTIAN };
            }
            else if (choose < .95f)
            {                // metal
                m = { glm::vec3(.5f + .5f * U(rng),
                                 .5f + .5f * U(rng),
                                 .5f + .5f * U(rng)),
                     .5f * U(rng),1,MAT_METAL };
            }
            else 
            {                                // glass
                m = { glm::vec3(1,1,1),0,1.5f, MAT_DIALECTRIC };
            }
            int mid = pushMat(m);
            hcenterRadii.push_back(glm::vec4(center, 0.2f));
            hSphereMatIDs.push_back(mid);
        }
    }

    //3 large spheres
    int glassId = pushMat({ glm::vec4(1,1,1,0),0,1.5f,MAT_DIALECTRIC });
    hcenterRadii.push_back(glm::vec4(0, 1, 0, 1));
    hSphereMatIDs.push_back(glassId);

    int lamId = pushMat({ glm::vec4(.4f,.2f,.1f,0),0,1,MAT_LAMBERTIAN });
    hcenterRadii.push_back(glm::vec4(-4, 1, 0, 1));
    hSphereMatIDs.push_back(lamId);

    int metalId = pushMat({ glm::vec4(.7f,.6f,.5f,0),0,1,MAT_METAL });
    hcenterRadii.push_back(glm::vec4(4, 1, 0, 1));
    hSphereMatIDs.push_back(metalId);

    //a plane
    int groundMaterial = pushMat({ glm::vec4(1.0f, 1.0f, 1.0f, 0), 0, 1, MAT_LAMBERTIAN });
    int groundMaterial2 = pushMat({ glm::vec3(1.0f, 1.0f, 1.0f), 0.01f, 1, MAT_METAL });
    hQuadMatIDs.push_back(groundMaterial);
    hQ.push_back(glm::vec3(-150.0f, 0.0f, -150.0f));
    hU.push_back(glm::vec3(0.0f, 0.0f, 300.0f));
    hV.push_back(glm::vec3(300.0f, 0.0f, 0.0f));

    //malloc scene
    cudaMallocManaged(&uScene, sizeof(Scene));

    //malloc materials
    uScene->materialCount = (int)hMats.size();
    cudaMallocManaged(&uScene->materials, hMats.size() * sizeof(MaterialData));
    memcpy(uScene->materials, hMats.data(), hMats.size() * sizeof(MaterialData));

    //malloc geometry
    //spheres
    cudaMallocManaged(&uScene->spheres, sizeof(SpheresPacked));
    cudaMallocManaged(&uScene->spheres->centerRadius, hcenterRadii.size() * sizeof(glm::vec4));
    cudaMallocManaged(&uScene->spheres->materialID, hSphereMatIDs.size() * sizeof(int));
    memcpy(uScene->spheres->centerRadius, hcenterRadii.data(), hcenterRadii.size() * sizeof(glm::vec4));
    memcpy(uScene->spheres->materialID, hSphereMatIDs.data(), hSphereMatIDs.size() * sizeof(int));
    uScene->spheres->n = (int)hcenterRadii.size();
    //planes
    cudaMallocManaged(&uScene->quads, sizeof(QuadsPacked));
    cudaMallocManaged(&uScene->quads->Q, hQ.size() * sizeof(glm::vec3));
    cudaMallocManaged(&uScene->quads->u, hU.size() * sizeof(glm::vec3));
    cudaMallocManaged(&uScene->quads->v, hV.size() * sizeof(glm::vec3));
    cudaMallocManaged(&uScene->quads->materialID, hQuadMatIDs.size() * sizeof(int));
    memcpy(uScene->quads->Q, hQ.data(), hQ.size() * sizeof(glm::vec3));
    memcpy(uScene->quads->u, hU.data(), hU.size() * sizeof(glm::vec3));
    memcpy(uScene->quads->v, hV.data(), hV.size() * sizeof(glm::vec3));
    memcpy(uScene->quads->materialID, hQuadMatIDs.data(), hQuadMatIDs.size() * sizeof(int));
    uScene->quads->n = (int)hQ.size();
    

    checkCudaErrors(cudaGetLastError());
    std::cout << "WORLD BUILT\n";
}

void DestroySceneCPU(Scene*& uScene)
{
    checkCudaErrors(cudaFree(uScene->spheres->centerRadius));
    checkCudaErrors(cudaFree(uScene->spheres->materialID));
    checkCudaErrors(cudaFree(uScene->spheres));
    checkCudaErrors(cudaFree(uScene->materials));
    checkCudaErrors(cudaFree(uScene));
}

void RenderScene(Scene uScene, Camera** dCamera, curandState* dRandomPixelStates)
{
    dim3 block(16, 16);
    dim3 grid(CeilDiv(SCREEN_WIDTH, block.x), CeilDiv(SCREEN_HEIGHT, block.y));

    //start timer
    auto t0 = std::chrono::high_resolution_clock::now();

    //launch render kernel
    std::cout << "STARTING RENDERING...\n";
    RenderKernel << <grid, block >> > (dCamera, uScene, dRandomPixelStates);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    //end timer
    auto t1 = std::chrono::high_resolution_clock::now();
    float secs = std::chrono::duration<float>(t1 - t0).count();
    std::cout << "GPU render pass (host-timed): "
        << secs << " secs\n";
}

void CleanUp(Scene* uScene, Camera** dCamera, unsigned char* dPixels, unsigned char* hPixels)
{
    DestroySceneCPU(uScene);

    DestroyCameraKernel << <1, 1 >> > (dCamera);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    checkCudaErrors(cudaFree(dPixels));
    checkCudaErrors(cudaFree(dCamera));
    delete[] hPixels;
}

void DisplayResult(unsigned char* hPixels)
{
    GLFWwindow* window = setupWindow();
    unsigned int quadVAO = setupBuffer();
    SetUpOpenGLState();
    Shader screenShader = Shader("shaders/screen.vert", "shaders/screen.frag");
    screenShader.use();
    screenShader.setInt("sceneTexture", 0);

    //convert host pixel buffer to openGL texture
    unsigned int resultTextureRGB = GetOGLTextureFromPixelBuffer(hPixels);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        screenShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, resultTextureRGB);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}

int main()
{
    //CONFIG
    Config config = MakeConfig();

    //device setup
    InitCUDA();

    //random seed
    int seed = (int)std::chrono::high_resolution_clock::now().time_since_epoch().count();

    //create device and host texture
    unsigned char* hPixels = new unsigned char[SCREEN_WIDTH * SCREEN_HEIGHT * 3];
    unsigned char* dPixels; checkCudaErrors(cudaMalloc(&dPixels, SCREEN_WIDTH * SCREEN_HEIGHT * 3));

    //create random states for each pixel
    curandState* dRandomPixelStates;
    SetupRandomPixelStates(dRandomPixelStates, seed);

    //create world on cpu and camera on the device
    Camera** dCamera;
    BuildCamera(dCamera, dPixels, config);

    //build world
    Scene* uScene;
    BuildSceneCPU(seed, uScene);

    //render
    RenderScene(*uScene, dCamera, dRandomPixelStates);

    //copy device texture into host texture
    checkCudaErrors(cudaMemcpy(hPixels, dPixels, SCREEN_WIDTH * SCREEN_HEIGHT * 3, cudaMemcpyDeviceToHost));

    //display result
    DisplayResult(hPixels);

    //free allocated  memory
    CleanUp(uScene, dCamera, dPixels, hPixels);

    return 0;
}
