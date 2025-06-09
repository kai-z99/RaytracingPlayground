#pragma once

#include "Hittable.h"
#include "Material.h"
#include "SetupHelper.h"
#include "Generic.h"

#include <iomanip>
#include <chrono>


class Camera
{
public:
    unsigned int Render(const Hittable& world)
    {
        Init();


        auto start = std::chrono::high_resolution_clock::now();

        for (int j = 0; j < SCREEN_HEIGHT; ++j)
        {
            for (int i = 0; i < SCREEN_WIDTH; ++i)
            {
                glm::dvec3 pixelColor = glm::dvec3(0.0);

                for (int sample = 0; sample < this->samplesPerPixel; sample++)
                {
                    Ray ray = this->GetRay(i, j);
                    pixelColor += this->RayColor(ray, this->maxRayDepth, world);
                }

                pixelColor /= this->samplesPerPixel;
                
                this->WriteColor(pixels, i, j, pixelColor);

            }

            PrintProgressBar(j+1, SCREEN_HEIGHT);
           
        }
            
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;

        std::cout << '\n';
        std::cout << "Elapsed time: " << duration.count() << " seconds\n";
        std::cout << "Total rays calculated: " << this->raysCalculated << '\n';


        unsigned int resultTextureRGB = setupTexture(pixels);
        return resultTextureRGB;
    }

private:
    glm::dvec3 center;
    unsigned char* pixels;
    float aspect;
    int samplesPerPixel;
    int maxRayDepth;

    //stats
    mutable int raysCalculated;

    void Init()
    {
        this->center = glm::dvec3(0.0,0.0,0.0);
        this->pixels = new unsigned char[SCREEN_WIDTH * SCREEN_HEIGHT * 3];
        this->aspect = SCREEN_WIDTH / float(SCREEN_HEIGHT);
        this->samplesPerPixel = 10;
        this->maxRayDepth = 50;

        this->raysCalculated = 0;
    }

    Ray GetRay(int i, int j) const
    {
        glm::dvec3 jitter = this->SampleSquare();

        double u = (i + 0.5 + jitter.x) / SCREEN_WIDTH;
        double v = (j + 0.5 + jitter.y) / SCREEN_HEIGHT;
        double ndcX = 2.0 * u - 1.0;
        double ndcY = 1.0 - 2.0 * v;
        ndcX *= aspect;

        glm::dvec3 dir = glm::normalize(glm::dvec3(ndcX, ndcY, -1.0f));
        
        return Ray(this->center, dir);
    }

    glm::dvec3 SampleSquare() const
    {
        return glm::dvec3(RandomDouble() - 0.5, RandomDouble() - 0.5, 0);
    }

    //moniter undos this
    inline double LinearToGamma(double linear)
    {
        const double gamma = 2.2;

        if (linear > 0)
        {
            return std::pow(linear, 1.0 / gamma);
        }

        return 0;
    }

    void WriteColor(unsigned char* pixelBuffer, int i, int j, glm::dvec3 color)
    {
        double r = LinearToGamma(color.r);
        double g = LinearToGamma(color.g);
        double b = LinearToGamma(color.b);

        //write color
        static const Interval intensity(0.000, 0.999);
        unsigned char rOut = (unsigned char)(intensity.Clamp(r) * 255.0f);
        unsigned char gOut = (unsigned char)(intensity.Clamp(g) * 255.0f);
        unsigned char bOut = (unsigned char)(intensity.Clamp(b) * 255.0f);

        int dst = (((SCREEN_HEIGHT - 1) - j) * SCREEN_WIDTH + i) * 3;

        pixelBuffer[dst + 0] = rOut;
        pixelBuffer[dst + 1] = gOut;
        pixelBuffer[dst + 2] = bOut;
    }

    
    glm::dvec3 RayColor(const Ray& r, int depth, const Hittable& world) const
    {
        if (depth <= 0) return glm::dvec3(0.0);

        this->raysCalculated++;

        HitRecord rec;

        if (world.Hit(r, Interval(0.001, infinity), rec))
        {
            Ray scattered;
            glm::dvec3 attenuation;

            if (rec.mat->Scatter(r, rec, attenuation, scattered))
            {
                return attenuation * RayColor(scattered, depth - 1, world);
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

    void PrintProgressBar(int current, int total) {
        const int barWidth = 50;
        double progress = (double)current / total;

        std::cout << "\r[";
        int pos = static_cast<int>(barWidth * progress);

        for (int i = 0; i < barWidth; ++i)
        {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }

        std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << " %" << std::flush;
    }
};