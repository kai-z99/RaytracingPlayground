#pragma once

#include "Generic.h"
#include "Scene.h"
#include "Material.h"

#include <vector>
#include <unordered_map>

struct Material
{
	MaterialGPU mat;	
	virtual ~Material() = default;
};

struct DiffuseMaterial : public Material
{
	DiffuseMaterial(glm::vec3 albedo = glm::vec3(1.0f), float roughness = 0.0f /*lambert */ )
	{
		this->mat.tag = DIFFUSE;
		this->mat.diffuse.albedo = albedo;
		this->mat.diffuse.roughness = roughness;
	}
};

struct MetalMaterial : public Material
{
	MetalMaterial(glm::vec3 albedo = glm::vec3(1.0f), float fuzz = 0.0f)
	{
		this->mat.tag = CONDUCTOR;
		this->mat.microfacet.albedo = albedo;
		this->mat.microfacet.roughness = fuzz;
		this->mat.microfacet.metallic = 1.0f;
	}
};

struct DielectricMaterial : public Material
{
	DielectricMaterial(glm::vec3 albedo = glm::vec3(1.0f), float eta = 1.5f, float roughness = 0.1f)
	{
		this->mat.tag = DIELECTRIC;
		this->mat.dielectric.eta = eta;
		this->mat.dielectric.roughness = roughness;
	}
};

struct DiffuseLightMaterial : public Material
{
	DiffuseLightMaterial(glm::vec3 emissive = glm::vec3(1.0f))
	{
		mat.tag = EMISSIVE;
		mat.emissive.emission = emissive;
	}
};

struct SubsurfaceMaterial : public Material
{
	SubsurfaceMaterial
	(
		glm::vec3 albedo = glm::vec3(1.0f),
		float radius = 1.0f,
		float eta = 1.5f,
		float roughness = 0.5f
	)
	{
		mat.subsurface.albedo = albedo;
		mat.subsurface.ell = radius;
		mat.subsurface.eta = eta;
		mat.subsurface.roughness = roughness;
		mat.tag = SUBSURFACE;

	}
};


class SceneBuilder
{
public:
	SceneBuilder() = default;

	//Returns a fully unified memory allocated scene object.
	Scene* Build();

	void AddSphere(glm::vec3 position, float radius, const Material& material);
	//u-v quad
	void AddQuad(glm::vec3 position, glm::vec3 u, glm::vec3 v, const Material& material);
	//centered quad
	void AddQuad(glm::vec3 position, glm::vec2 size, glm::vec4 rotation, const Material& material);

	void AddBox(glm::vec3 center, glm::vec3 size, glm::vec4 rotation, const Material& material);

	void AddTriangle(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, const Material& material);

	void AddModel(const std::string& path, const glm::mat4& transform, const Material& material);

private:
	void UploadMaterialDataToScene(Scene*& scene);
	std::vector<MaterialGPU> materials;
	int PushMaterialAndGetID(MaterialGPU m);

	//sphere
	void UploadSphereDataToScene(Scene*& scene);
	std::vector<glm::vec4> spherePositionRadii;
	std::vector<int> sphereMaterialIDs;

	//quads
	void UploadQuadDataToScene(Scene*& scene);
	std::vector<glm::vec3> quadQs;
	std::vector<glm::vec3> quadUs;
	std::vector<glm::vec3> quadVs;
	std::vector<int> quadMaterialsIDs;

	//tris
	void UploadTriangleDataToScene(Scene*& scene);
	std::vector<glm::vec3> triP0s;
	std::vector<glm::vec3> triP1s;
	std::vector<glm::vec3> triP2s;
	std::vector<int> triMaterialsIDs;

	//lighting
	void BuildLightSet(Scene*& scene);

};