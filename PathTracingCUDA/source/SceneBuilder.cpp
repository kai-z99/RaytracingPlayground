#include "../include/SceneBuilder.h"
#include "../include/Primitives.h"

#include <glm/gtc/quaternion.hpp>

#include <iostream>


Scene* SceneBuilder::Build()
{
	std::cout << "BUILDING WORLD...\n";

	Scene* scene;

	//malloc scene
	cudaMallocManaged(&scene, sizeof(Scene));

	//all materials
	this->UploadMaterialDataToScene(scene);

	//sphere data
	this->UploadSphereDataToScene(scene);

	//quad data
	this->UploadQuadDataToScene(scene);

	//tri data
	this->UploadTriangleDataToScene(scene);
	
	std::cout << "WORLD BUILT!\n";

	std::cout << "CONSTRUCTING BVH...\n";
	BuildBVH(
		*scene->spheres,
		*scene->quads,
		*scene->tris,
		scene->BVHNodes,
		scene->BVHCount,
		scene->primTypes,
		scene->primIndices);

	std::cout << "CONSTRUCTED BVH!\n";

	return scene;
}

void SceneBuilder::UploadSphereDataToScene(Scene*& scene)
{
	//malloc managed all sphere fields
	cudaMallocManaged(&scene->spheres, sizeof(SpheresPacked));
	cudaMallocManaged(&scene->spheres->centerRadius, this->spherePositionRadii.size() * sizeof(glm::vec4));
	cudaMallocManaged(&scene->spheres->materialID, this->sphereMaterialIDs.size() * sizeof(int));
	memcpy(scene->spheres->centerRadius, this->spherePositionRadii.data(), this->spherePositionRadii.size() * sizeof(glm::vec4));

	//copy our vector data into the gpu memory
	memcpy(scene->spheres->materialID, this->sphereMaterialIDs.data(), this->sphereMaterialIDs.size() * sizeof(int));
	scene->spheres->n = (int)this->spherePositionRadii.size();
}

void SceneBuilder::UploadQuadDataToScene(Scene*& scene)
{
	//malloc managed all quad fields
	cudaMallocManaged(&scene->quads, sizeof(QuadsPacked));
	cudaMallocManaged(&scene->quads->Q, this->quadQs.size() * sizeof(glm::vec3));
	cudaMallocManaged(&scene->quads->u, this->quadUs.size() * sizeof(glm::vec3));
	cudaMallocManaged(&scene->quads->v, this->quadVs.size() * sizeof(glm::vec3));
	cudaMallocManaged(&scene->quads->materialID, this->quadMaterialsIDs.size() * sizeof(int));

	//copy our vector data into the gpu memory
	memcpy(scene->quads->Q, this->quadQs.data(), this->quadQs.size() * sizeof(glm::vec3));
	memcpy(scene->quads->u, this->quadUs.data(), this->quadUs.size() * sizeof(glm::vec3));
	memcpy(scene->quads->v, this->quadVs.data(), this->quadVs.size() * sizeof(glm::vec3));
	memcpy(scene->quads->materialID, this->quadMaterialsIDs.data(), this->quadMaterialsIDs.size() * sizeof(int));
	scene->quads->n = (int)this->quadQs.size();

	
}

void SceneBuilder::UploadTriangleDataToScene(Scene*& scene)
{
	//malloc managed all tri fields
	cudaMallocManaged(&scene->tris, sizeof(TrianglesPacked));
	cudaMallocManaged(&scene->tris->p0, this->triP0s.size() * sizeof(glm::vec3));
	cudaMallocManaged(&scene->tris->p1, this->triP1s.size() * sizeof(glm::vec3));
	cudaMallocManaged(&scene->tris->p2, this->triP2s.size() * sizeof(glm::vec3));
	cudaMallocManaged(&scene->tris->materialID, this->triMaterialsIDs.size() * sizeof(int));

	//copy our vector data into the gpu memory
	memcpy(scene->tris->p0, this->triP0s.data(), this->triP0s.size() * sizeof(glm::vec3));
	memcpy(scene->tris->p1, this->triP1s.data(), this->triP1s.size() * sizeof(glm::vec3));
	memcpy(scene->tris->p2, this->triP2s.data(), this->triP2s.size() * sizeof(glm::vec3));
	memcpy(scene->tris->materialID, this->triMaterialsIDs.data(), this->triMaterialsIDs.size() * sizeof(int));
	scene->tris->n = (int)this->triP0s.size();
}

void SceneBuilder::UploadMaterialDataToScene(Scene*& scene)
{
	//malloc managed material fields
	cudaMallocManaged(&scene->materials, this->materials.size() * sizeof(MaterialData));

	//copy our vector data into the gpu memory
	memcpy(scene->materials, this->materials.data(), this->materials.size() * sizeof(MaterialData));
	scene->materialCount = (int)this->materials.size();
}

void SceneBuilder::AddSphere(glm::vec3 position, float radius, const Material& material)
{
	this->spherePositionRadii.push_back(glm::vec4(position, radius));

	int matID = PushMaterialAndGetID(material.ToMaterialData());
	this->sphereMaterialIDs.push_back(matID);
}

void SceneBuilder::AddQuad(glm::vec3 position, glm::vec3 u, glm::vec3 v, const Material& material)
{
	this->quadQs.push_back(position);
	this->quadUs.push_back(u);
	this->quadVs.push_back(v);

	int matID = PushMaterialAndGetID(material.ToMaterialData());
	this->quadMaterialsIDs.push_back(matID);

}

void SceneBuilder::AddQuad(glm::vec3 position, glm::vec2 size, glm::vec4 rotation, const Material& material)
{
	glm::vec3 axis = glm::normalize(glm::vec3(rotation));
	float angleRad = glm::radians(rotation.w);
	glm::quat q = glm::angleAxis(angleRad, axis);

	glm::vec3 u = q * glm::vec3(size.x, 0.0f, 0.0f);
	glm::vec3 v = q * glm::vec3(0.0f, 0.0f, size.y);

	glm::vec3 origin = position - (0.5f * u) - (0.5f * v);

	AddQuad(origin, u, v, material);
}

void SceneBuilder::AddTriangle(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, const Material& material)
{
	this->triP0s.push_back(p0);
	this->triP1s.push_back(p1);
	this->triP2s.push_back(p2);

	int matID = PushMaterialAndGetID(material.ToMaterialData());
	this->triMaterialsIDs.push_back(matID);
}

void SceneBuilder::AddModel(const std::string& path, const glm::mat4& transform, const Material& material)
{

	//LOAD OBJ
}



int SceneBuilder::PushMaterialAndGetID(MaterialData m)
{
	this->materials.push_back(m);
	return (int)(this->materials.size() - 1);
}
