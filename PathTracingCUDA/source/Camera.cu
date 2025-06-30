#include "../include/Camera.h"

#include "../include/Hittable.h"
#include "../include/Material.h"
#include "../include/SetupHelper.h"
#include "../include/Generic.h"
#include "../include/CudaHelper.h"

#include <iomanip>
#include <chrono>


__device__ Camera::Camera()
{
    this->Init();
}

__device__ void Camera::RenderPixel(curandState& randState, const Hittable& world, unsigned int i, unsigned int j)
{
    glm::dvec3 pixelColor = glm::dvec3(0.0);

    for (int sample = 0; sample < this->samplesPerPixel; sample++)
    {
        Ray ray = this->GetRay(randState, i, j);
        pixelColor += this->RayColorIter(randState, ray, this->maxRayDepth, world);
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
    this->up = glm::dvec3(0, 1, 0);
    this->center = glm::dvec3(13, 2, 3);
    this->lookAt = glm::dvec3(0, 0, 0);
    this->vfov = 20;
    this->focalLength = glm::length(center - lookAt);

    this->w = glm::normalize(center - lookAt);
    this->u = glm::normalize(glm::cross(up, w));
    this->v = glm::cross(w, u);

    this->aspect = SCREEN_WIDTH / float(SCREEN_HEIGHT);
    this->halfHeight = std::tan(glm::radians(this->vfov) * 0.5);
    this->halfWidth = this->aspect * this->halfHeight;
    this->samplesPerPixel = 10;
    this->maxRayDepth = 20;
}

__device__ Ray Camera::GetRay(curandState& randState, int i, int j) const
{
    glm::dvec3 jitter = this->SampleSquare(randState);

    double uS = (i + 0.5 + jitter.x) / SCREEN_WIDTH;
    double vS = (j + 0.5 + jitter.y) / SCREEN_HEIGHT;
    double ndcX = (2.0 * uS - 1.0) * this->halfWidth * this->focalLength;
    double ndcY = (1.0 - 2.0 * vS) * this->halfHeight * this->focalLength;

    glm::dvec3 dir = glm::normalize(glm::dvec3(this->u * ndcX + this->v * ndcY - this->w * focalLength));

    return Ray(this->center, dir);
}

__device__ glm::dvec3 Camera::SampleSquare(curandState& randState) const
{
    return glm::dvec3(RandomDouble(randState) - 0.5, RandomDouble(randState) - 0.5, 0);
}

//moniter undos this
__device__ inline double Camera::LinearToGamma(double linear)
{
    const double gamma = 2.2;

    if (linear > 0)
    {
        return std::pow(linear, 1.0 / gamma);
    }

    return 0;
}

__device__ void Camera::WriteColor(unsigned char* pixelBuffer, int i, int j, glm::dvec3 color)
{
    double r = LinearToGamma(color.r);
    double g = LinearToGamma(color.g);
    double b = LinearToGamma(color.b);

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

__device__ glm::dvec3 Camera::RayColor(curandState& randState, const Ray& r, int depth, const Hittable& world) const
{
    if (depth <= 0) return glm::dvec3(0.0);

    HitRecord rec;

    if (world.Hit(r, Interval(0.001, infinity), rec))
    {
        Ray scattered;
        glm::dvec3 attenuation;

        if (rec.mat->Scatter(randState, r, rec, attenuation, scattered))
        {
            return attenuation * RayColor(randState, scattered, depth - 1, world);
        }
        else //absorbed, etc
        {
            return glm::dvec3(0.0);
        }

    }

    double a = 0.5 * (r.direction().y + 1.0);

    glm::dvec3 white = glm::vec3(1.0, 1.0, 1.0);
    glm::dvec3 sky = glm::vec3(0.5, 0.7, 1.0);
    glm::dvec3 col = (1.0 - a) * white + a * sky;

    return col;
}

__device__ glm::dvec3 Camera::RayColorIter(curandState& randState, Ray r, int maxDepth, const Hittable& world) const
{
    glm::dvec3 col(0.0);      
    glm::dvec3 totalAttenuation(1.0);          

    for (int depth = 0; depth < maxDepth; ++depth)
    {
        HitRecord rec;
        if (!world.Hit(r, Interval(0.001, infinity), rec)) //hit sky
        {   
            double a = 0.5 * (r.direction().y + 1.0);
            glm::dvec3 sky = (1.0 - a) * glm::dvec3(1.0)
                + a * glm::dvec3(0.5, 0.7, 1.0);
            col += totalAttenuation * sky;
            break;
        }

        Ray scattered;
        glm::dvec3 attenuation;

        if (!rec.mat->Scatter(randState, r, rec, attenuation, scattered)) //absorbed
        {  
            break;
        }

        totalAttenuation *= attenuation;  
        r = scattered;    
    }
    return col;
}
