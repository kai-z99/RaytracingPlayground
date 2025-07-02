# Ray Tracing Performance Testing

## Render Configuration
- Resolution: **800×600 pixels**
- Rays per Pixel: **10**
- Maximum Bounce Depth: **12**

## Performance Results

| Platform     | Acceleration | Time         |
|--------------|--------------|--------------|
| CPU          | No BVH       | ~30 minutes  |
| CPU          | BVH          | ~142 seconds |
| GPU (CUDA)   | No BVH       | ~12.4 seconds|
| GPU (CUDA)   | BVH          | ~3.45 seconds|

## Notes
- BVH = Bounding Volume Hierarchy (used for acceleration)
- GPU implementation leverages CUDA for parallelism
- Scene is same from final output of "Ray Tracing in One Weekend" by Peter Shirley and more. https://raytracing.github.io/books/RayTracingInOneWeekend.html

![Render Output](Images/rayShowCUDA1.png)