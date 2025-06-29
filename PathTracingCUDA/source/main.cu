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

//todo: implement rand
//todo: implenent bvh



__global__ void BuildWorldKernel(Hittable** outWorld)
{
    if (threadIdx.x == 0 && threadIdx.y == 0)
    {
        HittableList* world = new HittableList();

        Material* mGround = new Lambertian(glm::dvec3(0.5, 0.5, 0.5));
        world->Add(new Sphere(glm::dvec3(0, -1000, 0),
            1000.0,
            mGround));

        //
        // 2) Add the random small spheres in a grid
        //
        for (int a = -11; a < 11; a++) {
            for (int b = -11; b < 11; b++) {
                double chooseMat = 0.5;
                glm::dvec3 center(
                    a + 0.9 * 0.5,
                    0.2,
                    b + 0.9 * 0.5
                );

                if (glm::length(center - glm::dvec3(4, 0.2, 0)) > 0.9) {
                    Material* sphereMat;

                    if (chooseMat < 0.8) {
                        // diffuse
                        glm::dvec3 albedo = glm::dvec3(0.5, 0.5, 0.5) * glm::dvec3(0.5, 0.5, 0.5);
                        sphereMat = new Lambertian(albedo);
                    }
                    else if (chooseMat < 0.95) {
                        // metal
                        glm::dvec3 albedo(
                            0.5,
                            0.5,
                            0.5
                        );
                        double fuzz = 0.5;
                        sphereMat = new Metal(albedo, fuzz);
                    }
                    else {
                        // glass
                        sphereMat = new Dialectric(1.5);
                    }

                    world->Add(new Sphere(center,
                        0.2,
                        sphereMat));
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

__global__ void RenderKernel(Camera** camera, Hittable** world)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= SCREEN_WIDTH || j >= SCREEN_HEIGHT) return;
    (*camera)->RenderPixel(**world, i, j);
}

__global__ void DestroyKernel(Camera** camera, Hittable** world)
{
    if (threadIdx.x == 0 && blockIdx.x == 0)
    {
        delete *camera;  delete *world;
    }
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
    //create device and host texture
    unsigned char* hPixels = new unsigned char[SCREEN_WIDTH * SCREEN_HEIGHT * 3];
    unsigned char* dPixels;
    checkCudaErrors(cudaMalloc(&dPixels, SCREEN_WIDTH * SCREEN_HEIGHT * 3));

    //create world and camera on the device
    Camera** dCamera;
    Hittable** dWorld;
    checkCudaErrors(cudaMalloc(&dCamera, sizeof(Camera*)));
    checkCudaErrors(cudaMalloc(&dWorld, sizeof(Hittable*)));
    size_t heap = 64 * 1024 * 1024;   //64mb
    cudaDeviceSetLimit(cudaLimitMallocHeapSize, heap);
    BuildWorldKernel<<<1,1>>>(dWorld);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());
    BuildCameraKernel<<<1,1>>>(dCamera, dPixels);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    //launch render kernel
    dim3 block(16, 16);
    dim3 grid(CeilDiv(SCREEN_WIDTH, block.x), CeilDiv(SCREEN_HEIGHT, block.y));
    cudaDeviceSetLimit(cudaLimitStackSize, 16384); //for recursion...
    RenderKernel<<<grid, block>>>(dCamera, dWorld);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

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
