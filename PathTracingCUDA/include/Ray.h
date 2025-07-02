#define GLM_FORCE_CUDA
#include <glm/glm.hpp>
#include "CudaHelper.h"

class Ray
{
public:
	__device__ Ray() = default;

	__device__ Ray(const glm::vec3& origin, const glm::vec3 direction) : orig(origin), dir(direction) {};

	__device__ const glm::vec3& origin() const { return this->orig; }
	__device__ const glm::vec3& direction() const { return this->dir; }

	__device__ glm::vec3 at(float t) const
	{
		return orig + t * dir;
	}

private:
	glm::vec3 orig;
	glm::vec3 dir;
};