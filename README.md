## Notes
- BVH = Bounding Volume Hierarchy (used for acceleration)
- The BVH is built with a median-split strategy along the longest axis of each AABB.
- This approach delivers very fast build times (≈O(n log n)), but is less optimized for ray-tracing cost. A binned Surface Area Heuristic (SAH) pass will improve traversal performance further.

## Render Configuration
- GPU: **RTX 4070 Mobile** | CPU: **i7-13650HX**
- Resolution: **800×600 pixels**
- Rays per Pixel: **10**
- Maximum Bounce Depth: **12**
- Scene: **Same from final output of "Ray Tracing in One Weekend" by Peter Shirley and more. https://raytracing.github.io/books/RayTracingInOneWeekend.html**

## Performance Results

| Platform     | Acceleration | Time         |
|--------------|--------------|--------------|
| CPU          | N/A          | ~1800 seconds|
| CPU          | BVH          | ~142 seconds |
| GPU (CUDA)   | N/A          | ~12.4 seconds|
| GPU (CUDA)   | BVH, Recursive Traversal | ~3.45 seconds|
| GPU (CUDA)   | BVH, Flat Traversal | ~1.65 seconds|



![Render Output](Images/rayShowCUDA1.png)
(Image uses 1000spp)


## Render Configuration
- GPU: **RTX 4070 Mobile** | CPU: **i7-13650HX**
- Resolution: **1920×1080 pixels**
- Rays per Pixel: **1000**
- Maximum Bounce Depth: **12**
- Scene: **Cornell Box, assorted material spheres**
## Performance Results

| Platform     | Acceleration | Time         |
|--------------|--------------|--------------|
| GPU (CUDA)   | BVH, Flat Traversal | ~535 seconds|

![Render Output](Images/rayShowCUDA5.png)
