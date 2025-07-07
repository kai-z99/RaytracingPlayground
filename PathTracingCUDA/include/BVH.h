#pragma once

#include "Primitives.h"

#include <vector>
#include <algorithm>
#include <cfloat>

//Naive: ~1700 seconds
//Random: 137.1 seconds
//Longest Axis: 62.3 seconds

//REFACTOR

struct BVHNode
{
	glm::vec3 bboxMin;
	glm::vec3 bboxMax;
	uint32_t leftFirst; //INNER: index of left node in BVHNodes* (right is +1) . LEAF: index of first prim in primtypes*/indices*
	uint32_t rightFirst;
	uint32_t primCount; //0 means inner node, >0 means leaf node. (Note we dont always have 1 prim per leaf as the cost of traversing will be higher than collision detection)

	//the goal is that when the traversel hits a leaf, it refers you to a spot in the parralell arrays primIndices* and primTypes* to do the collision.
	//Note that prims in the same leaf will ALWAYS be contigous in the paralell arrays. This is why primCount will simply let you traverse forward 
	// n times and hit all the prims in the leaf.
};

//first implement longest axis bvh build
//then implement binned SAH


struct PrimRef
{
	glm::vec3 bboxMin, bboxMax;
	glm::vec3 centroid;
	PrimType type;
	uint32_t index;
};


inline uint32_t BuildNode(std::vector<PrimRef>& prims,
	std::vector<BVHNode>& nodes,
	std::vector<PrimType>& leafTypes,
	std::vector<uint32_t>& leafIdx,
	uint32_t first,
	uint32_t last);

//we can safely pass in scene->BVHNodes, scene->nodeCount.. etc to this function.
//We can also pass in SpherePacked and QuadsPacked that were malloced by cudaMallocManaged. perfect.
inline void BuildBVH(
	const SpheresPacked& spheres,
	const QuadsPacked& quads,
	BVHNode*& outNodes,
	uint32_t& outNodeCount,
	PrimType*& outPrimTypes,
	uint32_t*& outPrimIndices)
{
	std::vector<PrimRef> prims;
	prims.reserve(spheres.n + quads.n);

	//fill spheres
	for (uint32_t i = 0; i < spheres.n; i++)
	{
		const glm::vec4 cr = spheres.centerRadius[i];
		glm::vec3 c(cr);
		float r = cr.w;
		PrimRef p;
		p.bboxMin = c - glm::vec3(r);
		p.bboxMax = c + glm::vec3(r);
		p.centroid = c;
		p.type = PRIM_SPHERE;
		p.index = i;
		prims.push_back(p);
	}

	//fill quads
	for (uint32_t i = 0; i < quads.n; i++)
	{
		glm::vec3 Q = quads.Q[i];
		glm::vec3 u = quads.u[i];
		glm::vec3 v = quads.v[i];
		glm::vec3 p0 = Q;
		glm::vec3 p1 = Q + u;
		glm::vec3 p2 = Q + v;
		glm::vec3 p3 = Q + u + v;

		PrimRef p;
		p.bboxMin = glm::min(glm::min(p0, p1), glm::min(p2,p3));
		p.bboxMax = glm::max(glm::max(p0, p1), glm::max(p2,p3));
		p.centroid = 0.25f * (p0 + p1 + p2 + p3);
		p.type = PRIM_QUAD;
		p.index = i;
		prims.push_back(p);

	}

	//create storage
	std::vector<BVHNode> nodes;
	nodes.reserve(2 * prims.size() - 1);
	std::vector<PrimType> leafTypes;  
	leafTypes.reserve(prims.size());
	std::vector<uint32_t> leafIdx;    
	leafIdx.reserve(prims.size());

	//build nodes throught the whole range
	int build = BuildNode(prims, nodes, leafTypes, leafIdx,
		0, static_cast<uint32_t>(prims.size()));

	if (build == UINT32_MAX)
	{
		std::cout << "WARNING: BVH NOT CONTRUCTED\n";
		return;
	}

	//malloc the bvh node array
	outNodeCount = static_cast<uint32_t>(nodes.size());
	cudaMallocManaged(&outNodes, outNodeCount * sizeof(BVHNode));
	memcpy(outNodes, nodes.data(), outNodeCount * sizeof(BVHNode));

	//malloc the parallel arrays
	uint32_t primTotal = static_cast<uint32_t>(leafIdx.size());
	cudaMallocManaged(&outPrimTypes, primTotal * sizeof(PrimType));
	cudaMallocManaged(&outPrimIndices, primTotal * sizeof(uint32_t));
	memcpy(outPrimTypes, leafTypes.data(), primTotal * sizeof(PrimType));
	memcpy(outPrimIndices, leafIdx.data(), primTotal * sizeof(uint32_t));

}

inline uint32_t BuildNode(std::vector<PrimRef>& prims,
	std::vector<BVHNode>& nodes,
	std::vector<PrimType>& leafTypes,
	std::vector<uint32_t>& leafIdx,
	uint32_t first,
	uint32_t last)
{
	if (first == last) return UINT32_MAX; //no good

	uint32_t nodeIdx = static_cast<uint32_t>(nodes.size());
	nodes.emplace_back();

	BVHNode& node = nodes.back();

	glm::vec3 bbMin(+FLT_MAX), bbMax(-FLT_MAX);
	for (uint32_t i = first; i < last; i++)
	{
		bbMin = glm::min(bbMin, prims[i].bboxMin);
		bbMax = glm::max(bbMax, prims[i].bboxMax);
	}
	node.bboxMin = bbMin;
	node.bboxMax = bbMax;

	uint32_t count = last - first;
	const uint32_t MAX_PRIMS_PER_LEAF = 4;

	//If we decide we are at a leaf, push into the parallel arrays and exit.
	if (count <= MAX_PRIMS_PER_LEAF)
	{
		node.leftFirst = static_cast<uint32_t>(leafIdx.size());
		node.rightFirst = UINT32_MAX; //wasted
		node.primCount = count;

		for (uint32_t i = first; i < last; i++)
		{
			leafTypes.push_back(prims[i].type);
			leafIdx.push_back(prims[i].index);
		}
		return nodeIdx;
	}

	//since we arent at a leaf yet, choose the axis and split

	//choose split axis (longest extent of centroids)
	glm::vec3 cMin(+FLT_MAX), cMax(-FLT_MAX);
	for (uint32_t i = first; i < last; i++)
	{
		cMin = glm::min(cMin, prims[i].centroid);
		cMax = glm::max(cMax, prims[i].centroid);
	}
	int axis = 0;
	glm::vec3 extent = cMax - cMin;
	if (extent.y > extent.x && extent.y > extent.z) axis = 1;
	if (extent.z > extent.x && extent.z > extent.y) axis = 2;
	if (extent[axis] < 1e-6f) axis = -1;

	if (axis < 0) {                      // fall-back leaf
		node.leftFirst = static_cast<uint32_t>(leafIdx.size());
		node.rightFirst = UINT32_MAX; //wasted
		node.primCount = count;
		for (uint32_t i = first; i < last; ++i) {
			leafTypes.push_back(prims[i].type);
			leafIdx.push_back(prims[i].index);
		}
		return nodeIdx;
	}

	//partially sort 
	uint32_t mid = (first + last) >> 1;
	std::nth_element(
		prims.begin() + first,
		prims.begin() + mid,
		prims.begin() + last,
		[axis](const PrimRef& a, const PrimRef& b)
		{ return a.centroid[axis] < b.centroid[axis]; });

	uint32_t left = BuildNode(prims, nodes, leafTypes, leafIdx, first, mid);
	uint32_t right = BuildNode(prims, nodes, leafTypes, leafIdx, mid, last);

	node.leftFirst = left;
	node.rightFirst = right;
	node.primCount = 0;

	return nodeIdx;
}