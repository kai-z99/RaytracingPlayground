#pragma once

#include "Scene.h"
#include "Camera.h"

namespace Scenes
{
	Scene* RayTracingInOneWeekend(int seed, Camera*& cam);

	Scene* TriangleTestScene(int seed, Camera*& cam);

	Scene* PlaneTestScene(int seed, Camera*& cam);

	Scene* CornellBoxScene(int seed, Camera*& cam);

	Scene* SlabScene(int seed, Camera*& cam);

	Scene* SkyScene(int seed, Camera*& cam);

	Scene* MetalHeadScene(int seed, Camera*& cam);

	Scene* HeadScene(int seed, Camera*& cam);

	Scene* HandScene(int seed, Camera*& cam);

	Scene* ModelTest(int seed, Camera*& cam);

	Scene* StatueScene(int seed, Camera*& cam);

	Scene* PBRTest(int seed, Camera*& cam);

	Scene* CornellBoxOGScene(int seed, Camera*& cam);

	Scene* PassthroughScene(int seed, Camera*& cam);

	Scene* DragonScene(int seed, Camera*& cam);

	Scene* CheckerScene(int /*seed*/, Camera*& cam);
}