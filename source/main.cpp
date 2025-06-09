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

    Material* mGround = new Lambertian(glm::dvec3(0.8, 0.8, 0.0));
    Material* mCenter = new Lambertian(glm::dvec3(0.1, 0.2, 0.5));
    Material* mLeft = new Metal(glm::dvec3(0.8, 0.8, 0.8), 0.8);
    Material* mRight = new Metal(glm::dvec3(0.8, 0.6, 0.2), 0.0);


    world.Add(new Sphere(glm::dvec3(0, -100.5, -1.0), 100.0, mGround));
    world.Add(new Sphere(glm::dvec3(0, 0, -1.2), 0.5, mCenter));
    world.Add(new Sphere(glm::dvec3(-1.0, 0.0, -1.0), 0.5, mLeft));
    world.Add(new Sphere(glm::dvec3(1.0, 0.0, -1.0), 0.5, mRight));

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
