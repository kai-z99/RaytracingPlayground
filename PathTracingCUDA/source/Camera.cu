#include "../include/Camera.h"
#include "../include/Material.h"

//10:12 TESTING
//NAIVE: ~12.74
//BVH: ~3.45s

__host__ Camera::Camera()
{
    this->center = glm::vec3(0.0f, 0.0f, -2.0f);
    this->lookAt = glm::vec3(0.0f);
    this->vfov = 90.0f;
    this->backgroundColor = glm::vec3(0.70, 0.80, 1.00);
    this->russianroulette = true;

    this->Init();
}

__device__ void Camera::RenderPixel(curandState& randState, const Scene& scene, unsigned int i, unsigned int j)
{
    glm::vec3 pixelColor = glm::vec3(0.0f);

    for (int sample = 0; sample < this->samplesPerPixel; sample++)
    {
        Ray ray = this->GetRay(randState, i, j);
        pixelColor += this->RayColorIter(randState, ray, this->maxRayDepth, scene);
    }

    pixelColor /= this->samplesPerPixel;

    this->WriteColor(pixelBuffer, i, j, pixelColor);
}

__host__ void Camera::SetPixelBuffer(unsigned char* buffer)
{
    this->pixelBuffer = buffer;
}

__host__ void Camera::Init()
{
    this->up = glm::vec3(0, 1, 0);
    this->focalLength = glm::length(center - lookAt);

    this->w = glm::normalize(center - lookAt);
    this->u = glm::normalize(glm::cross(up, w));
    this->v = glm::cross(w, u);

    this->aspect = SCREEN_WIDTH / float(SCREEN_HEIGHT);
    this->halfHeight = tan(glm::radians(this->vfov) * 0.5);
    this->halfWidth = this->aspect * this->halfHeight;
}

__device__ Ray Camera::GetRay(curandState& randState, int i, int j) const
{
    glm::vec3 jitter = this->SampleSquare(randState);

    float uS = (i + 0.5 + jitter.x) / SCREEN_WIDTH;
    float vS = (j + 0.5 + jitter.y) / SCREEN_HEIGHT;
    float ndcX = (2.0 * uS - 1.0) * this->halfWidth * this->focalLength;
    float ndcY = (1.0 - 2.0 * vS) * this->halfHeight * this->focalLength;

    glm::vec3 dir = glm::normalize(glm::vec3(this->u * ndcX + this->v * ndcY - this->w * focalLength));

    return Ray(this->center, dir);
}

__device__ glm::vec3 Camera::SampleSquare(curandState& randState) const
{

    return glm::vec3(RandomFloat(randState) - 0.5, RandomFloat(randState) - 0.5, 0);
}

//moniter undos this
__device__ inline float Camera::LinearToGamma(float linear)
{
    const float gamma = 2.2;

    if (linear > 0)
    {
        return std::pow(linear, 1.0 / gamma);
    }

    return 0;
}

__device__ void Camera::WriteColor(unsigned char* pixelBuffer, int i, int j, glm::vec3 color)
{
    float r = LinearToGamma(color.r);
    float g = LinearToGamma(color.g);
    float b = LinearToGamma(color.b);

    //write color
    Interval intensity(0.000, 0.999);
    unsigned char rOut = (unsigned char)(intensity.Clamp(r) * 255.0f);
    unsigned char gOut = (unsigned char)(intensity.Clamp(g) * 255.0f);
    unsigned char bOut = (unsigned char)(intensity.Clamp(b) * 255.0f);

    int dst = (((SCREEN_HEIGHT - 1) - j) * SCREEN_WIDTH + i) * 3;

    pixelBuffer[dst + 0] = rOut;
    pixelBuffer[dst + 1] = gOut;
    pixelBuffer[dst + 2] = bOut;
}

__device__ glm::vec3 Camera::RayColorIter(curandState& randState, Ray r, int maxDepth, const Scene& scene) const
{
    glm::vec3 col(0.0f);
    glm::vec3 totalAttenuation(1.0f);

    bool prevSSS = false;

    for (int depth = 0; depth < maxDepth; ++depth)
    {
        HitRecord rec;

        //found source: sky
        if (!HitScene(scene, r, Interval(0.001f, infinity), rec)) //hit sky
        {
            col += totalAttenuation * this->backgroundColor;
            break;
        }

        MaterialData& materialData = scene.materials[rec.matDataID];

        //found source: area light
        if (materialData.type == MAT_LIGHT_DIFFUSE)
        {
            col += totalAttenuation * materialData.emission;
            break;
        }

        //surface hit, scatter?

        Ray scattered;
        glm::vec3 evaluation;
        float pdf;

        //float VdotN = fmaxf(glm::dot(-r.direction(), rec.normal), 0.0f);
        //float F_o = 1.0f - FrDielectricExact(VdotN, 1.0f, materialData.refractionIndex); //(1 - Fo) term. for light leaving surface

        if (!Scatter(materialData, randState, scene, r, rec, evaluation, scattered, pdf, prevSSS))
        {
            //surface has absorbed ray, exit
            break;
        }

        //scattered and attenuated succesfully---

        //estimate rendering equation for 1 monte carlo sample
        //Note: if its a sss surface, we evaluate the seperable bssrdf instead where evaluation is bssrdf * bsdf of initial hit
        float cosine = fmaxf(glm::dot(scattered.direction(), rec.normal), 0.0f);
        glm::vec3 contrib = evaluation * cosine / pdf;
        if (prevSSS) 
        {
            // measure “energy” as luma(contrib), or you could sum channels individually
            atomicAdd(&sssEnergySumR, (double)contrib.r);
            atomicAdd(&sssEnergySumG, (double)contrib.g);
            atomicAdd(&sssEnergySumB, (double)contrib.b);
            atomicAdd(&sssHitCount, 1ull);

            
        }
       /*contrib.r = fminf(contrib.r, 1.5f);
       contrib.g = fminf(contrib.g, 1.5f);
       contrib.b = fminf(contrib.b, 1.5f);*/

        totalAttenuation *= contrib;

        
        if (!isfinite(pdf))
        {
            printf("warning: pdf\n");
            break;
        }
        if (!isfinite(cosine))
        {
            printf("warning: cosine\n");
            break;
        }
        if (!isfinite(evaluation.r) || !isfinite(evaluation.g) || !isfinite(evaluation.b))
        {
            printf("warning: atttenuation\n");
            break;
        }

        if (!isfinite(contrib.r) || !isfinite(contrib.g) || !isfinite(contrib.b))
        {
            printf("warning: contrib\n");
            break;
        }

        

        if (this->russianroulette && depth >= 3)
        {
            //the max compoenent of total attenution
            float p = fminf(fmaxf(totalAttenuation.r, fmaxf(totalAttenuation.g, totalAttenuation.b)), 0.95f);

            //We lose energy when we terminate
            if (RandomFloat(randState) > p) break;

            //To preserve energy: boost the energy of non-terminated paths by the probablity of being terminated.
            totalAttenuation /= p; 
        }

        r = scattered;
    }

    return col;
}
