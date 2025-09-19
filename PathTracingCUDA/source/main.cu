#include "../include/SetupHelper.h"
#include "../include/Shader.h"
#include "../include/Generic.h"
#include "../include/Camera.h"
#include "../include/Scene.h"
#include "../include/ScenePresets.h"
#include "../include/Material.h"

#include <chrono>
#include <random>
#include <cstring>
struct Config
{
    int samplesPerPixel;
    int maxBounceDepth;
};

Config MakeConfig()
{
    Config c;
    c.samplesPerPixel = 128;
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
    InitPixelRandStatesKernel << <grid, block >> > (dPixelRandomStates, seed);
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

static cudaEvent_t RenderSceneAsync(Scene uScene, Camera uCamera, curandState* dRandomPixelStates, cudaStream_t renderStream)
{
    dim3 block(16, 16);
    dim3 grid(CeilDiv(SCREEN_WIDTH, block.x), CeilDiv(SCREEN_HEIGHT, block.y));

    // launch render kernel asynchronously on its own stream
    cudaEvent_t doneEvent;
    checkCudaErrors(cudaEventCreateWithFlags(&doneEvent, cudaEventDisableTiming));
    RenderKernel <<<grid, block, 0, renderStream>>>(uCamera, uScene, dRandomPixelStates); //launch kernel on the renderStream (before cuda event record)
    checkCudaErrors(cudaGetLastError());
    checkCudaErrors(cudaEventRecord(doneEvent, renderStream)); //make doneEvent success only when renderStream kernels that were queued before this eventRecord was called are finished
    return doneEvent;

    //no block (devicesync)
}

void CleanUp(Scene* uScene, Camera* uCamera, unsigned char* dPixels, unsigned char* hPixels)
{
    DestroySceneCPU(uScene);
    cudaDeviceSynchronize();
    checkCudaErrors(cudaGetLastError());

    checkCudaErrors(cudaFree(dPixels));
    checkCudaErrors(cudaFree(uCamera));
    checkCudaErrors(cudaFreeHost(hPixels));
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

static void DisplayResultProgressive(unsigned char* hPixels, unsigned char* dPixels, cudaEvent_t renderDoneEvent, cudaStream_t copyStream)
{
    GLFWwindow* window = setupWindow();
    unsigned int quadVAO = setupBuffer();
    SetUpOpenGLState();
    Shader screenShader = Shader("shaders/screen.vert", "shaders/screen.frag");
    screenShader.use();
    screenShader.setInt("sceneTexture", 0);

    // create OpenGL texture; initialize with current host buffer
    unsigned int resultTextureRGB = GetOGLTextureFromPixelBuffer(hPixels);

    const size_t byteCount = size_t(SCREEN_WIDTH) * size_t(SCREEN_HEIGHT) * 3;

    using clock = std::chrono::high_resolution_clock;
    auto lastBlit = clock::now() - std::chrono::seconds(2); // force an immediate first blit
    bool renderFinished = false;

    while (!glfwWindowShouldClose(window) && !renderFinished) {
        glfwPollEvents();

        // every 25ms, blit the current device pixel buffer to the screen
        auto now = clock::now();
        if (now - lastBlit >= std::chrono::milliseconds(25))
        {
            // copy device -> host asynchronously, then sync the copy stream
            checkCudaErrors(cudaMemcpyAsync(hPixels, dPixels, byteCount, cudaMemcpyDeviceToHost, copyStream));
            checkCudaErrors(cudaStreamSynchronize(copyStream));

            // update GL texture with new host pixels
            glBindTexture(GL_TEXTURE_2D, resultTextureRGB);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, hPixels);

            lastBlit = now;
        }

        // check if render finished
        // renderDoneEvent will be cudaSuccess only when the kernel on its stream finishes.
        if (!renderFinished && cudaEventQuery(renderDoneEvent) == cudaSuccess)
        {
            renderFinished = true;
            // one final copy to ensure we have the final image
            checkCudaErrors(cudaMemcpyAsync(hPixels, dPixels, byteCount, cudaMemcpyDeviceToHost, copyStream));
            checkCudaErrors(cudaStreamSynchronize(copyStream));
            glBindTexture(GL_TEXTURE_2D, resultTextureRGB);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, hPixels);
        }

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
    unsigned char* hPixels;
    checkCudaErrors(cudaMallocHost(&hPixels, SCREEN_WIDTH * SCREEN_HEIGHT * 3)); // pinned for async copies
    std::memset(hPixels, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 3);
    unsigned char* dPixels; checkCudaErrors(cudaMalloc(&dPixels, SCREEN_WIDTH * SCREEN_HEIGHT * 3));

    //create random states for each pixel
    curandState* dRandomPixelStates;
    SetupRandomPixelStates(dRandomPixelStates, seed);

    //create world on cpu and camera on the device
    Camera* uCamera;
    BuildCamera(uCamera, dPixels, config);

    //init scene
    Scene* uScene = Scenes::CornellBoxScene(seed, uCamera);

    // launch render async and display while running
    cudaStream_t renderStream; checkCudaErrors(cudaStreamCreate(&renderStream));
    cudaStream_t copyStream;   checkCudaErrors(cudaStreamCreate(&copyStream));

    std::cout << "STARTING RENDERING...\n";
    auto t0 = std::chrono::high_resolution_clock::now();
    cudaEvent_t doneEvent = RenderSceneAsync(*uScene, *uCamera, dRandomPixelStates, renderStream);

    // display and blit every ~1s while render runs
    DisplayResultProgressive(hPixels, dPixels, doneEvent, copyStream);

    // ensure render is done before cleanup & timing
    checkCudaErrors(cudaEventSynchronize(doneEvent));
    auto t1 = std::chrono::high_resolution_clock::now();
    float secs = std::chrono::duration<float>(t1 - t0).count();
    std::cout << "GPU render pass (host-timed, async): " << secs << " secs\n";

    DisplayResult(hPixels);

    // destroy CUDA sync objects
    checkCudaErrors(cudaEventDestroy(doneEvent));
    checkCudaErrors(cudaStreamDestroy(renderStream));
    checkCudaErrors(cudaStreamDestroy(copyStream));

    //free allocated  memory
    CleanUp(uScene, uCamera, dPixels, hPixels);

    return 0;
}