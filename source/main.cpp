#include "../include/SetupHelper.h"
#include "../include/Shader.h"

#include "../include/Generic.h"
#include "../include/Camera.h"
#include "../include/Hittable.h"
#include "../include/HittableList.h"
#include "../include/Sphere.h"

glm::vec3 rayColor(const Ray& r, const Hittable& world)
{
    HitRecord rec;

    if (world.Hit(r, Interval(0, infinity), rec))
    {
        return 0.5 * (rec.normal + glm::dvec3(1,1,1));
    }

    double a = 0.5 * (r.direction().y + 1.0);

    glm::dvec3 white = glm::vec3(1.0, 1.0, 1.0);
    glm::dvec3 sky = glm::vec3(0.5, 0.7, 1.0);
    glm::dvec3 col = (1.0 - a) * white + a * sky;

    return col;
}

void writeColor(unsigned char* pixelBuffer, int i, int j, glm::dvec3 color)
{
    //write color
    unsigned char r = (unsigned char)(color.r * 255.0f);
    unsigned char g = (unsigned char)(color.g * 255.0f);
    unsigned char b = (unsigned char)(color.b * 255.0f);

    int dst = (((SCREEN_HEIGHT - 1) - j) * SCREEN_WIDTH + i) * 3;

    pixelBuffer[dst + 0] = r;
    pixelBuffer[dst + 1] = g;
    pixelBuffer[dst + 2] = b;
}

int main()
{
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
    unsigned char* pixels = new unsigned char[SCREEN_WIDTH * SCREEN_HEIGHT * 3];
    float aspect = SCREEN_WIDTH / float(SCREEN_HEIGHT);

    glm::dvec3 cameraCenter = glm::dvec3(0.0,0.0,0.0);


    HittableList world;

    //
    //Material* mGround = new Lambertian(glm::dvec3(0.8, 0.8, 0.0));
    //Material* mCenter = new Lambertian(glm::dvec3(0.1, 0.2, 0.5));
    //Material* mLeft = new Dialectric(1.5);
    //Material* mBubble = new Dialectric(1/ 1.5);
    //Material* mRight = new Metal(glm::dvec3(0.8, 0.6, 0.2), 0.0);
    //
    //
    //world.Add(new Sphere(glm::dvec3(0, -100.5, -1.0), 100.0, mGround));
    //world.Add(new Sphere(glm::dvec3(0, 0, -1.2), 0.5, mCenter));
    //world.Add(new Sphere(glm::dvec3(-1.0, 0.0, -1.0), 0.5, mLeft));
    //world.Add(new Sphere(glm::dvec3(-1.0, 0.0, -1.0), 0.4, mBubble));
    //world.Add(new Sphere(glm::dvec3(1.0, 0.0, -1.0), 0.5, mRight));
    //

    
    Material* mGround = new Lambertian(glm::dvec3(0.5, 0.5, 0.5));
    world.Add(new Sphere(glm::dvec3(0, -1000, 0),
        1000.0,
        mGround));
    
    //
    // 2) Add the random small spheres in a grid
    //
    for (int a = -3; a < 3; a++) {
        for (int b = -3; b < 3; b++) {
            double chooseMat = RandomDouble();
            glm::dvec3 center(
                a + 0.9 * RandomDouble(),
                0.2,
                b + 0.9 * RandomDouble()
            );

            if (glm::length(center - glm::dvec3(4, 0.2, 0)) > 0.9) {
                Material* sphereMat;

                if (chooseMat < 0.8) {
                    // diffuse
                    glm::dvec3 albedo = glm::dvec3(RandomDouble(), RandomDouble(), RandomDouble()) * glm::dvec3(RandomDouble(), RandomDouble(), RandomDouble());
                    sphereMat = new Lambertian(albedo);
                }
                else if (chooseMat < 0.95) {
                    // metal
                    glm::dvec3 albedo(
                        RandomDouble(0.5, 1.0),
                        RandomDouble(0.5, 1.0),
                        RandomDouble(0.5, 1.0)
                    );
                    double fuzz = RandomDouble(0.0, 0.5);
                    sphereMat = new Metal(albedo, fuzz);
                }
                else {
                    // glass
                    sphereMat = new Dialectric(1.5);
                }

                world.Add(new Sphere(center,
                    0.2,
                    sphereMat));
            }
        }
    }
    
    //
    // 3) Add the three large spheres
    //
    Material* mCenter = new Dialectric(1.5);
    world.Add(new Sphere(glm::dvec3(0, 1, 0),
        1.0,
        mCenter));

    Material* mLeft = new Lambertian(glm::dvec3(0.4, 0.2, 0.1));
    world.Add(new Sphere(glm::dvec3(-4, 1, 0),
        1.0,
        mLeft));

    Material* mRight = new Metal(glm::dvec3(0.7, 0.6, 0.5),
        0.0);
    world.Add(new Sphere(glm::dvec3(4, 1, 0),
        1.0,
        mRight));

        
    Camera cam;

    unsigned int resultTextureRGB = cam.Render(world);


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
