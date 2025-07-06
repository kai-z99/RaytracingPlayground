#include "../include/WorldBuilder.h"

Scene* WorldBuilder::Build(int seed)
{
	Scene* scene;

	//malloc scene
	cudaMallocManaged(&scene, sizeof(Scene));







	return nullptr;
}

void WorldBuilder::AddSphere(glm::vec3 position, float radius, Material material)
{
	MaterialData m;
}

void WorldBuilder::AddQuad(glm::vec3 position, glm::vec2 size, Material material)
{
}

int WorldBuilder::CreateMaterialAndGetID(MaterialData& m)
{
	this->materials.push_back(m);
	return this->materials.size() - 1;
}
