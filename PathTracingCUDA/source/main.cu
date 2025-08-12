#include "../include/SetupHelper.h"
#include "../include/Shader.h"
#include "../include/Generic.h"
#include "../include/Camera.h"
#include "../include/Scene.h"
#include "../include/ScenePresets.h"
#include "../include/Material.h"

#include <chrono>
#include <random>

//Todo: convert to single axis pdfs for now because its techincally unbiased.

struct Config
{
    int samplesPerPixel;
    int maxBounceDepth;
};

//27.16
// 53.3681 secs
Config MakeConfig()
{
    Config c;
    c.samplesPerPixel = 100;
    c.maxBounceDepth = 15;

    std::cout << "CUDA VERSION" << '\n';
    std::cout << "RESOLUTION: " << std::to_string(SCREEN_WIDTH) << "x" << std::to_string(SCREEN_HEIGHT) << "px\n";
    std::cout << "SAMPLES PER PIXEL: " << c.samplesPerPixel << '\n';
    std::cout << "MAX BOUNCE DEPTH: " << c.maxBounceDepth << '\n';

    return c;
}

__global__ void RenderKernel(Camera camera, Scene scene, curandState* pixelRandStates)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= SCREEN_WIDTH || j >= SCREEN_HEIGHT) return;

    int pixelIndex = j * SCREEN_WIDTH + i;
    curandState randState = pixelRandStates[pixelIndex];

    (camera).RenderPixel(randState, scene, i, j);

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
    //cudaDeviceSetLimit(cudaLimitStackSize, 16384); //for recursion...
    //size_t heap = 64 * 1024 * 1024;   //64mb fir device news
    //cudaDeviceSetLimit(cudaLimitMallocHeapSize, heap); //for device new in BuildWorldKernel
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
    std::cout << "CREATED RANDOM STATES!\n";

}

void BuildCamera(Camera*& uCamera, unsigned char* pixelBuffer, const Config& config)
{
    cudaMallocManaged(&uCamera, sizeof(Camera));
    new (uCamera) Camera();                                  
    uCamera->samplesPerPixel = config.samplesPerPixel;
    uCamera->maxRayDepth = config.maxBounceDepth;
    uCamera->SetPixelBuffer(pixelBuffer);                           // still device mem
}


void DestroySceneCPU(Scene*& uScene)
{
    //sph
    checkCudaErrors(cudaFree(uScene->spheres->centerRadius));
    checkCudaErrors(cudaFree(uScene->spheres->materialID));
    checkCudaErrors(cudaFree(uScene->spheres));

    //tri
    checkCudaErrors(cudaFree(uScene->tris->p0));
    checkCudaErrors(cudaFree(uScene->tris->p1));
    checkCudaErrors(cudaFree(uScene->tris->p2));
    checkCudaErrors(cudaFree(uScene->tris->materialID));
    checkCudaErrors(cudaFree(uScene->tris));

    //quads
    checkCudaErrors(cudaFree(uScene->quads->Q));
    checkCudaErrors(cudaFree(uScene->quads->u));
    checkCudaErrors(cudaFree(uScene->quads->v));
    checkCudaErrors(cudaFree(uScene->quads->materialID));
    checkCudaErrors(cudaFree(uScene->quads));

    //misc
    checkCudaErrors(cudaFree(uScene->materials));
    checkCudaErrors(cudaFree(uScene->BVHNodes));
    checkCudaErrors(cudaFree(uScene->primTypes));
    checkCudaErrors(cudaFree(uScene->primIndices));
    checkCudaErrors(cudaFree(uScene));
}

void RenderScene(Scene uScene, Camera uCamera, curandState* dRandomPixelStates)
{
    dim3 block(16, 16);
    dim3 grid(CeilDiv(SCREEN_WIDTH, block.x), CeilDiv(SCREEN_HEIGHT, block.y));

    //start timer
    auto t0 = std::chrono::high_resolution_clock::now();

    //launch render kernel
    std::cout << "STARTING RENDERING...\n";
    RenderKernel<<<grid, block>>>(uCamera, uScene, dRandomPixelStates);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    //end timer
    auto t1 = std::chrono::high_resolution_clock::now();
    float secs = std::chrono::duration<float>(t1 - t0).count();
    std::cout << "GPU render pass (host-timed): "
        << secs << " secs\n";
}

void CleanUp(Scene* uScene, Camera* uCamera, unsigned char* dPixels, unsigned char* hPixels)
{
    DestroySceneCPU(uScene);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    checkCudaErrors(cudaFree(dPixels));
    checkCudaErrors(cudaFree(uCamera));
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
    Camera* uCamera;
    BuildCamera(uCamera, dPixels, config);

    Scene* uScene = Scenes::SkyScene(seed, uCamera);

    //render
    RenderScene(*uScene, *uCamera, dRandomPixelStates);

    printf("rejected: %i, total: %i, rptcg: %f, no material: %i\n", rejected, total, (float)rejected/(float)total, noIntersection);
    printf("uSum: %f, uAvg: %f \n", uSum, uSum / radialSamplesCount);
    printf("total radial samples: %f, total radii: %f, avg: %f, expected: %f\n", radialSamplesCount, radialSamplesSum, radialSamplesSum / radialSamplesCount, expectedRadialAverage);
    printf("SSS samples: %llu, Avg Energy: (%lf, %lf, %lf)\n", sssHitCount, sssEnergySumR/(double)sssHitCount, sssEnergySumG / (double)sssHitCount, sssEnergySumB / (double)sssHitCount);

    //NOTEL: SSS enrgy expceted is supposed to be == sss.Tint. For some reason, increasing the sssRadius increasees total ebergy and vise versa. Interesetingly, 
    // A good hint is the energy discrpency is uniform across rgb, for example if tint = (1.0f, 0.4, 0.5) and the R avg was 4.4, dividing G and B by 4.4 should make R,G = 0.4f, 0.4f, and 
    //obvously dividing R by 4.4 will give 1, recovering (1,0.4,0.4). Note again this energy discrepency increases and decreases based on increasing and decereasing radius.
    //Some tested Average energy values for each sssradius. for tint = (1, 0.4, 0.4), therfore expected is (1, 0.4, 0.4) no matter the sssRadius. Note that for this case since 
    // tint.r = 1, energy.r is exactly the discrepency for all of energy.rgb (uniform).
    // 0.1f: (inf, inf, inf)
    // 0.15f:(inf, inf, inf)
    // 0.2f: (1.990812, 0.796325, 0.796325)
    // 0.3f: (2.139567, 0.855827, 0.855827)
    // 0.4f: (2.274573, 0.909829, 0.909829)
    // 1.0f: (2.669819, 1.067928, 1.067928)
    // 5.0f: (3.708132, 1.483253, 1.483253)
    // 10.0f:(4.235995, 1.694398, 1.694398)

    //copy device texture into host texture
    checkCudaErrors(cudaMemcpy(hPixels, dPixels, SCREEN_WIDTH * SCREEN_HEIGHT * 3, cudaMemcpyDeviceToHost));

    //display result
    DisplayResult(hPixels);

    //free allocated  memory
    CleanUp(uScene, uCamera, dPixels, hPixels);

    return 0;
}
