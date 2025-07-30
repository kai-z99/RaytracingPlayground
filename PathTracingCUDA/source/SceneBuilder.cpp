#include "../include/SceneBuilder.h"
#include "../include/Primitives.h"

#include <glm/gtc/quaternion.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include "../include/tiny_obj_loader.h"


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

void SceneBuilder::AddBox(glm::vec3 center,
	glm::vec3 size,
	glm::vec4 rotation,
	const Material& material)
{
	// --- build a quaternion ------------------------------------------------
	glm::vec3 axis = glm::normalize(glm::vec3(rotation));   // (x,y,z)
	float     ang = glm::radians(rotation.w);              
	glm::quat q = glm::angleAxis(ang, axis);

	// --- oriented edge vectors (full length, not half!) --------------------
	glm::vec3 U = q * glm::vec3(size.x, 0.0f, 0.0f);        // “right”
	glm::vec3 V = q * glm::vec3(0.0f, size.y, 0.0f);        // “up”
	glm::vec3 W = q * glm::vec3(0.0f, 0.0f, size.z);        // “forward”

	// For each face: pick a centre, then call AddQuad(origin,u,v)
	auto addFace = [&](glm::vec3 faceCenter,
		glm::vec3 u, glm::vec3 v)
		{
			glm::vec3 origin = faceCenter - 0.5f * u - 0.5f * v;
			AddQuad(origin, u, v, material);
		};

	addFace(center + 0.5f * W, U, V);

	addFace(center - 0.5f * W, -U, V);

	addFace(center + 0.5f * U, -W, V);

	addFace(center - 0.5f * U, W, V);

	addFace(center + 0.5f * V, U, -W);

	addFace(center - 0.5f * V, U, W);
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
	int tris = 0;
	std::cout << "LOADING MODEL AT: " << path << '\n';

	MaterialData   md = material.ToMaterialData();
    int            meshMatID = PushMaterialAndGetID(md);

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> mats;
	std::string warn, err;
	tinyobj::LoadObj(&attrib, &shapes, &mats, &warn, &err, path.c_str());

	//compute aabb
	glm::vec3 vmin(FLT_MAX), vmax(-FLT_MAX);
	for (size_t i = 0; i < attrib.vertices.size() / 3; i++)
	{
		//get the triangle
		glm::vec3 v
		(
			attrib.vertices[3 * i + 0],
			attrib.vertices[3 * i + 1],
			attrib.vertices[3 * i + 2]

		);

		vmin = glm::min(vmin, v);
		vmax = glm::max(vmax, v);
	}

	//compute scale factor for 1x1
	glm::vec3 extent = vmax - vmin;
	float scale = 1.0f / glm::max(extent.x, glm::max(extent.y, extent.z)); // 1 / maxBoxDimension
	glm::vec3 center = (vmin + vmax) / 2.0f;

	std::cout << "X EXTENT: " << extent.x * scale << '\n';
	std::cout << "Y EXTENT: " << extent.y * scale << '\n';
	std::cout << "Z EXTENT: " << extent.z * scale << '\n';

	//build transform
	glm::mat4 T = glm::translate(glm::mat4(1.0f), -center); //shift the model so that its geometric center is (0,0,0) in model space.
	glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale)); //that way we can uniformally scale it like this.
	glm::mat4 M = transform * S * T;

	//triangulate and add
	for (const tinyobj::shape_t& shape : shapes)
	{
		size_t offset = 0;

		for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
		{
			int fv = shape.mesh.num_face_vertices[f];

			//fan-triangulate
			for (int v = 1; v + 1 < fv; v++)
			{
				glm::vec3 tri[3];
				int idxs[3] = {0, v, v + 1};
				for (int k = 0; k < 3; k++)
				{
					tinyobj::index_t idx = shape.mesh.indices[offset + idxs[k]];
					tri[k] = glm::vec3
					(
						attrib.vertices[3 * idx.vertex_index + 0],
						attrib.vertices[3 * idx.vertex_index + 1],
						attrib.vertices[3 * idx.vertex_index + 2]
					);
				}

				//apply M then add
				glm::vec3 p0 = glm::vec3(M * glm::vec4(tri[0], 1.0f));
				glm::vec3 p1 = glm::vec3(M * glm::vec4(tri[1], 1.0f));
				glm::vec3 p2 = glm::vec3(M * glm::vec4(tri[2], 1.0f));

				this->triP0s.push_back(p0);
				this->triP1s.push_back(p1);
				this->triP2s.push_back(p2);

				this->triMaterialsIDs.push_back(meshMatID); //dont push a new duplicate material for every triangle
				tris++;
			}

			offset += fv;
		}
	}

	std::cout << "MODEL LOADED. TRIANGLES: " << tris << '\n';
}



int SceneBuilder::PushMaterialAndGetID(MaterialData m)
{
	this->materials.push_back(m);
	return (int)(this->materials.size() - 1);
}
