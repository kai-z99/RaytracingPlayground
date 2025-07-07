#include "../include/SceneBuilder.h"
#include "../include/Primitives.h"


Scene* SceneBuilder::Build()
{
	Scene* scene;

	//malloc scene
	cudaMallocManaged(&scene, sizeof(Scene));

	//all materials
	this->UploadMaterialDataToScene(scene);

	//sphere data
	this->UploadSphereDataToScene(scene);

	//quad data
	this->UploadQuadDataToScene(scene);
	

	BuildBVH(
		*scene->spheres,
		*scene->quads,
		scene->BVHNodes,
		scene->BVHCount,
		scene->primTypes,
		scene->primIndices);

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

int SceneBuilder::PushMaterialAndGetID(MaterialData m)
{
	this->materials.push_back(m);
	return (int)(this->materials.size() - 1);
}
