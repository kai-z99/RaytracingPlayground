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

//todo: implement rand
//todo: implenent bvh
//todo: implement printing


__global__ void BuildWorldKernel(Hittable** outWorld)
{
    if (*outWorld) return;

    if (threadIdx.x == 0 && threadIdx.y == 0)
    {
        //make a private rand state
        curandState randState;
        curand_init(2025, 0, 0, &randState);

        HittableList* world = new HittableList();

        Material* mGround = new Lambertian(glm::dvec3(0.5, 0.5, 0.5));
        world->Add(new Sphere(glm::dvec3(0, -1000, 0),
            1000.0,
            mGround));

        for (int a = -11; a < 11; a++) 
        {
            for (int b = -11; b < 11; b++) 
            {
                double chooseMat = RandomDouble(randState);
                glm::dvec3 center
                (
                    a + 0.9 * RandomDouble(randState),
                    0.2,
                    b + 0.9 * RandomDouble(randState)
                );

                if (glm::length(center - glm::dvec3(4, 0.2, 0)) > 0.9) 
                {
                    Material* sphereMat;

                    if (chooseMat < 0.8) 
                    {
                        // diffuse
                        glm::dvec3 albedo = RandomVec3Positive(randState) * RandomVec3Positive(randState);
                        sphereMat = new Lambertian(albedo);
                    }
                    else if (chooseMat < 0.95) 
                    {
                        // metal
                        glm::dvec3 albedo
                        (
                            RandomDouble(randState, 0.5, 1.0),
                            RandomDouble(randState, 0.5, 1.0),
                            RandomDouble(randState, 0.5, 1.0)
                        );
                        double fuzz = 0.5;
                        sphereMat = new Metal(albedo, fuzz);
                    }
                    else 
                    {
                        // glass
                        sphereMat = new Dialectric(1.5);
                    }

                    world->Add(new Sphere(center, 0.2, sphereMat));
                }
            }
        }

        //
        // 3) Add the three large spheres
        //
        Material* mCenter = new Dialectric(1.5);
        world->Add(new Sphere(glm::dvec3(0, 1, 0),
            1.0,
            mCenter));

        Material* mLeft = new Lambertian(glm::dvec3(0.4, 0.2, 0.1));
        world->Add(new Sphere(glm::dvec3(-4, 1, 0),
            1.0,
            mLeft));

        Material* mRight = new Metal(glm::dvec3(0.7, 0.6, 0.5),
            0.0);
        world->Add(new Sphere(glm::dvec3(4, 1, 0),
            1.0,
            mRight));

        *outWorld = world;
    }
}

__global__ void BuildCameraKernel(Camera** outCamera, unsigned char* pixels)
{
    if (threadIdx.x == 0 && threadIdx.y == 0)
    {
        *outCamera = new Camera();
        (*outCamera)->SetPixelBuffer(pixels);
    }
}

__global__ void RenderKernel(Camera** camera, Hittable** world, curandState* pixelRandStates)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= SCREEN_WIDTH || j >= SCREEN_HEIGHT) return;

    int pixelIndex = j * SCREEN_WIDTH + i;
    curandState randState = pixelRandStates[pixelIndex];

    (*camera)->RenderPixel(randState, **world, i, j);

    //update the state after using it
    pixelRandStates[pixelIndex] = randState;

    if (threadIdx.x == 0 && threadIdx.y == 0 && blockIdx.x == 0) 
    {
        float pct = 100.f * (blockIdx.y + 1) / gridDim.y;
        printf("Progress: %.1f%% (row %d of %d)\n", pct, blockIdx.y + 1, gridDim.y);
    }
}

__global__ void DestroyKernel(Camera** camera, Hittable** world)
{
    if (threadIdx.x == 0 && blockIdx.x == 0)
    {
        delete *camera;  delete *world;
    }
}

__global__ void InitPixelRandStatesKernel(curandState* randStates)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i >= SCREEN_WIDTH || j >= SCREEN_HEIGHT) return;

    int idx = j * SCREEN_WIDTH + i;

    curand_init(2025 + idx, 0, 0, &randStates[idx]);
}

int main()
{
    std::cout << "CUDA VERSION" << '\n';
    //-------------------------------------------------------
    //OPENGL STUFF------------------------------------
    //-------------------------------------------------------
    GLFWwindow* window = setupWindow();
    unsigned int quadVAO = setupBuffer();
    setupState();
    Shader screenShader = Shader("shaders/screen.vert","shaders/screen.frag");
    screenShader.use();
    screenShader.setInt("sceneTexture", 0);
    
    //-------------------------------------------------------
    //RAY CASTING STUFF------------------------------------
    //-------------------------------------------------------
    
    // grid and block size of screen
    dim3 block(16, 16);
    dim3 grid(CeilDiv(SCREEN_WIDTH, block.x), CeilDiv(SCREEN_HEIGHT, block.y));
    
    //create device and host texture
    unsigned char* hPixels = new unsigned char[SCREEN_WIDTH * SCREEN_HEIGHT * 3];
    unsigned char* dPixels;
    checkCudaErrors(cudaMalloc(&dPixels, SCREEN_WIDTH * SCREEN_HEIGHT * 3));

    //create random states
    curandState* dPixelRandomStates;
    cudaMalloc(&dPixelRandomStates, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(curandState)); //one for each pixel
    InitPixelRandStatesKernel<<<grid, block>>>(dPixelRandomStates); 
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    //create world and camera on the device
    Camera** dCamera;
    Hittable** dWorld;
    checkCudaErrors(cudaMalloc(&dCamera, sizeof(Camera*)));
    checkCudaErrors(cudaMalloc(&dWorld, sizeof(Hittable*)));
    size_t heap = 64 * 1024 * 1024;   //64mb
    cudaDeviceSetLimit(cudaLimitMallocHeapSize, heap); //for device new in BuildWorldKernel

    BuildWorldKernel<<<1,1>>>(dWorld);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    BuildCameraKernel<<<1,1>>>(dCamera, dPixels);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());
    auto t0 = std::chrono::high_resolution_clock::now();
    //launch render kernel
    cudaDeviceSetLimit(cudaLimitStackSize, 16384); //for recursion...
    RenderKernel<<<grid, block>>>(dCamera, dWorld, dPixelRandomStates);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    auto t1 = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "GPU render pass (host-timed): "
        << secs << " secs\n";

    //copy device texture into host texture
    checkCudaErrors(cudaMemcpy(hPixels, dPixels, SCREEN_WIDTH * SCREEN_HEIGHT * 3, cudaMemcpyDeviceToHost));

    //cleanup
    DestroyKernel<<<1,1>>>(dCamera, dWorld);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaFree(dPixels));
    checkCudaErrors(cudaFree(dCamera));
    checkCudaErrors(cudaFree(dWorld));

    //convert host pixel buffer to openGL texture
    unsigned int resultTextureRGB = setupTexture(hPixels);

    //-------------------------------------------------------
    //RENDER IMAGE------------------------------------
    //-------------------------------------------------------
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
    return 0;
}
