#pragma once

#include "Generic.h"
#include "Scene.h"

#include <vector>

struct Material
{
	glm::vec3 color;
	
	virtual ~Material() = default;
	virtual MaterialData ToMaterialData() const = 0;

protected:
	MaterialType tag;
};

struct LambertianMaterial : public Material
{
	LambertianMaterial(glm::vec3 color = glm::vec3(1.0f)) 
	{
		this->tag = MAT_LAMBERTIAN;
		this->color = color;
	}

	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.color = this->color;
		m.type = this->tag;
		return m;
	}
};

struct MetalMaterial : public Material
{
	float fuzz;

	MetalMaterial(glm::vec3 color = glm::vec3(1.0f), float fuzz = 0.0f)
	{
		this->tag = MAT_METAL;
		this->color = color;
		this->fuzz = fuzz;
	}
	
	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.color = this->color;
		m.fuzz = this->fuzz;
		m.type = this->tag;
		return m;
	}

};

struct DialectricMaterial : public Material
{
	float eta;

	DialectricMaterial(float eta = 1.5f)
	{
		this->tag = MAT_DIALECTRIC;
		this->color = glm::vec3(1.0f);
		this->eta = eta;
		
	}

	MaterialData ToMaterialData() const override
	{
		MaterialData m;
		m.color = this->color;
		m.refractionIndex = this->eta;
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


};