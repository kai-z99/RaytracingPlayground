## Notes
- BVH = Bounding Volume Hierarchy (used for acceleration)
  - The BVH is built with a median-split strategy along the longest axis of each AABB.
  - This approach delivers very fast build times (≈O(n log n)), but is less optimized for ray-tracing cost. A binned Surface Area Heuristic (SAH) pass will improve traversal performance further.
  - We traverse this tree structure on the GPU iteratively as opposed to recursively, as it maximizes register usage and minimizes branch divergence.
- Importance Sampling
  - We must sample biased towards the GGX normal distribution for proper convergence when rendering metallic objects.
  - Heitz (2018) describes an algorithm that samples only visible normals. Both Heitz's algorithm and classic NDF importance sampling are implemented.
  - https://jcgt.org/published/0007/04/01/paper.pdf
    
<img src="Images/1kggx.png" alt="GGX Render" width="350"/>
<img src="Images/1klambert.png" alt="Lambert Render" width="350"/>

- Above is when we use importance sampling, below is naive cosine sampling. Both images are rendered with 1000 samples per pixel.
  - Similarly for other BRDFs, use the correct importance sampling for proper convergence.



<br/>

## Render Configuration
- GPU: **RTX 4070 Mobile** | CPU: **i7-13650HX**
- Resolution: **1920×1080 pixels**
- Rays per Pixel: **10000**
- Maximum Bounce Depth: **15**
- Scene: **Cornell Box, Stanford Dragon (871306 triangles)**
## Performance Results

| Platform     | Acceleration | Time         |
|--------------|--------------|--------------|
| GPU (CUDA)   | BVH, Flat Traversal | ~999.8 seconds|

![Render Output](Images/rayShowCUDA10.png)
![Render Output](Images/rayShowCUDA22.png)
![Render Output](Images/rayShowCUDA12.png)

<br/>
<br/>

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
| GPU (CUDA)   | N/A          | ~0.28 seconds|
| GPU (CUDA)   | BVH, Flat Traversal | ~0.06 seconds|



![Render Output](Images/rayShowCUDA1.png)
(Image uses 1000spp)

<br/>
<br/>

## Other Images Gallery
<img width="1231" height="1108" alt="image" src="https://github.com/user-attachments/assets/4f89f2c6-73ef-4ee1-8eaa-3ea073080534" />
<img width="1918" height="1107" alt="rayShowCUDA30" src="https://github.com/user-attachments/assets/871cfc7e-5263-4f38-ab95-1df0f650a051" />
<img width="1227" height="1111" alt="rayShowCUDA24" src="https://github.com/user-attachments/assets/9a160bc1-a052-4ba3-8c54-490bcffb7d94" />
<img width="1228" height="1107" alt="rayShowCUDA27" src="https://github.com/user-attachments/assets/77079b7a-0800-4c81-b991-1cdda5f172ec" />





