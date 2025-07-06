#pragma once

#include "Generic.h"
#include "Scene.h"

#include <vector>

struct Material
{
	glm::vec3 color;

protected:
	MaterialType tag;
};

struct LambertianMaterial : public Material
{
	LambertianMaterial() 
	{
		this->tag = MAT_LAMBERTIAN;
	}
};

struct MetalMaterial : public Material
{
	MetalMaterial()
	{
		this->tag = MAT_METAL;
	}
	float fuzz;
};

struct DialectricMaterial : public Material
{
	float eta;
};


class WorldBuilder
{
public:
	WorldBuilder() = default;

	Scene* Build(int seed);

	void AddSphere(glm::vec3 position, float radius, Material material);
	void AddQuad(glm::vec3 position, glm::vec2 size, Material material);

private:
	std::vector<MaterialData> materials;
	int CreateMaterialAndGetID(MaterialData& m);

	//sphere
	std::vector<glm::vec4> spherePositionRadii;
	std::vector<int> sphereMaterialIDs;

	//quads
	std::vector<glm::vec3> quadQs;
	std::vector<glm::vec3> quadUs;
	std::vector<glm::vec3> quadVs;
	std::vector<int> quadMaterialsIDs;

};