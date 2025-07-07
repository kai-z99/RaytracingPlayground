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

class WorldBuilder
{
public:
	WorldBuilder() = default;

	//Returns a fully unified memory allocated scene object.
	Scene* Build(int seed);

	void AddSphere(glm::vec3 position, float radius, const Material& material);
	void AddQuad(glm::vec3 position, glm::vec3 u, glm::vec3 v, const Material& material);

private:
	std::vector<MaterialData> materials;
	int PushMaterialAndGetID(MaterialData m);

	//sphere
	std::vector<glm::vec4> spherePositionRadii;
	std::vector<int> sphereMaterialIDs;

	//quads
	std::vector<glm::vec3> quadQs;
	std::vector<glm::vec3> quadUs;
	std::vector<glm::vec3> quadVs;
	std::vector<int> quadMaterialsIDs;

};