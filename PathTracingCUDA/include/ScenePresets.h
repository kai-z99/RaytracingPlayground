#pragma once

#include "Scene.h"
#include "Camera.h"

namespace Scenes
{
	Scene* RayTracingInOneWeekend(int seed);

	Scene* KaisScene(int seed);

	Scene* TriangleTestScene(int seed);

	Scene* PlaneTestScene(int seed, Camera*& cam);
}