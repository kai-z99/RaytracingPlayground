#pragma once

#include "Scene.h"
#include "Camera.h"

namespace Scenes
{
	Scene* RayTracingInOneWeekend(int seed, Camera*& cam);

	Scene* KaisScene(int seed, Camera*& cam);

	Scene* TriangleTestScene(int seed, Camera*& cam);

	Scene* PlaneTestScene(int seed, Camera*& cam);
}