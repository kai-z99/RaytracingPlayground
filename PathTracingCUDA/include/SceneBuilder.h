#pragma once

#include "Generic.h"
#include "Scene.h"

#include <vector>
#include <unordered_map>

struct Material
{
	glm::vec3 albedo;
	
	virtual ~Material() = default;
	virtual MaterialData ToMaterialData() const = 0;

protected:
	MaterialType tag;
};

struct PBRMaterial : public Material
{
	glm::vec3 albedo;
	float metallic;
	float roughness;

	PBRMaterial(glm::vec3 albedo = glm::vec3(1.0f), float metallic = 0.0f, float roughness = 1.0f)
	{
		this->tag = MAT_PBR;
		this->albedo = albedo;
		this->metallic = metallic;
		this->roughness = roughness;
	}

	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.albedo = this->albedo;
		m.metallic = this->metallic;
		m.roughness = this->roughness;
		m.type = this->tag;
		return m;
	}
};

struct LambertianMaterial : public Material
{
	LambertianMaterial(glm::vec3 albedo = glm::vec3(1.0f))
	{
		this->tag = MAT_PBR;
		this->albedo = albedo;
	}

	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.albedo = this->albedo;
		m.roughness = 0.0f;
		m.metallic = 0.0f;
		m.type = this->tag;
		return m;
	}
};

struct MetalMaterial : public Material
{
	float fuzz;

	MetalMaterial(glm::vec3 albedo = glm::vec3(1.0f), float fuzz = 0.0f)
	{
		this->tag = MAT_PBR;
		this->albedo = albedo;
		this->fuzz = fuzz;
	}

	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.albedo = this->albedo;
		m.roughness = this->fuzz;
		m.metallic = 1.0f;
		m.type = this->tag;
		return m;
	}

};

struct DielectricMaterial : public Material
{
	float eta;

	DielectricMaterial(glm::vec3 albedo = glm::vec3(1.0f), float eta = 1.5f)
	{
		this->tag = MAT_DIALECTRIC;
		this->albedo = albedo;
		this->eta = eta;
		
	}

	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.albedo = this->albedo;
		m.refractionIndex = this->eta;
		m.type = this->tag;
		return m;
	}
};

struct DiffuseLightMaterial : public Material
{
	glm::vec3 emissive;

	DiffuseLightMaterial(glm::vec3 emissive = glm::vec3(1.0f))
	{
		this->tag = MAT_LIGHT_DIFFUSE;
		this->albedo = glm::vec3(0.0f);
		this->emissive = emissive;
	}

	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.albedo = this->albedo;
		m.emission = this->emissive;
		m.type = this->tag;

		return m;
	}
};

struct SubsurfaceMaterial : public Material
{
	float subsurface;
	glm::vec3 sssRadius;
	glm::vec3 sssTint;
	float eta;
	float roughness;
	float metallic;

	SubsurfaceMaterial
	(
		glm::vec3 albedo = glm::vec3(1.0f),
		glm::vec3 sssTint = glm::vec3(1.0f),
		float subsurface = 1.0f,
		glm::vec3 radius = glm::vec3(1.0f),
		float eta = 1.5f,
		float metallic = 0.0f,
		float roughness = 0.5f
	)
	{
		this->tag = MAT_SUBSURFACE;
		this->albedo = albedo;
		this->subsurface = subsurface;
		this->sssTint = sssTint;
		this->sssRadius = radius;
		this->eta = eta;
		this->roughness = roughness;
		this->metallic = metallic;
	}

	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.albedo = this->albedo;
		m.sssTint = this->sssTint;
		m.sssRadius = this->sssRadius;
		m.refractionIndex = this->eta;
		m.subsurface = this->subsurface;
		m.roughness = this->roughness;
		m.metallic = this->metallic;
		m.type = this->tag;

		return m;
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

	void AddTriangle(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, const Material& material);

	void AddModel(const std::string& path, const glm::mat4& transform, const Material& material);

private:
	void UploadMaterialDataToScene(Scene*& scene);
	std::vector<MaterialData> materials;
	int PushMaterialAndGetID(MaterialData m);

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

	//models

};