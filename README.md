# Ray Tracing Performance Testing

## Render Configuration
- Resolution: **800×600 pixels**
- Rays per Pixel: **10**
- Maximum Bounce Depth: **12**

## Performance Results

| Platform     | Acceleration | Time         |
|--------------|--------------|--------------|
| CPU          | N/A          | ~1800 seconds|
| CPU          | BVH          | ~142 seconds |
| GPU (CUDA)   | N/A          | ~12.4 seconds|
| GPU (CUDA)   | BVH, Recursive Traversal | ~3.45 seconds|
| GPU (CUDA)   | BVH, Flat Traversal | ~1.65 seconds|

## Notes
- BVH = Bounding Volume Hierarchy (used for acceleration)
- GPU implementation leverages CUDA for parallelism
- Scene is same from final output of "Ray Tracing in One Weekend" by Peter Shirley and more. https://raytracing.github.io/books/RayTracingInOneWeekend.html

![Render Output](Images/rayShowCUDA1.png)
