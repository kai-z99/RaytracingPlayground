#include "../include/Camera.h"
#include "../include/Material.h"

//10:12 TESTING
//NAIVE: ~12.74
//BVH: ~3.45s

__device__ Camera::Camera()
{
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

__device__ void Camera::SetPixelBuffer(unsigned char* buffer)
{
    this->pixelBuffer = buffer;
}

__device__ void Camera::Init()
{
    this->up = glm::vec3(0, 1, 0);
    this->center = glm::vec3(13, 2, 3);
    //this->center = glm::vec3(0, 5, 0);
    this->lookAt = glm::vec3(0, 0, 0);
    this->vfov = 20;

    this->focalLength = glm::length(center - lookAt);

    this->w = glm::normalize(center - lookAt);
    this->u = glm::normalize(glm::cross(up, w));
    this->v = glm::cross(w, u);

    this->aspect = SCREEN_WIDTH / float(SCREEN_HEIGHT);
    this->halfHeight = tan(glm::radians(this->vfov) * 0.5);
    this->halfWidth = this->aspect * this->halfHeight;
    this->samplesPerPixel = 1; //user set
    this->maxRayDepth = 1; //user set
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

    for (int depth = 0; depth < maxDepth; ++depth)
    {
        HitRecord rec;
        if (!HitScene(scene, r, Interval(0.001f, infinity), rec)) //hit sky
        {
            float a = 0.5f * (r.direction().y + 1.0f);
            glm::vec3 sky = (1.0f - a) * glm::vec3(1.0f)
                + a * glm::vec3(0.5f, 0.7f, 1.0f);
            col += totalAttenuation * sky;
            break;
        }

        Ray scattered;
        glm::vec3 attenuation;

        MaterialData& materialData = scene.materials[rec.matDataID];
        if (!Scatter(materialData, randState, r, rec, attenuation, scattered))
        {
            break;
        }

        totalAttenuation *= attenuation;
        r = scattered;
    }
    return col;
}
