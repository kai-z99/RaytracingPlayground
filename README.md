<img width="1918" height="1111" alt="sssAssorted50000spp" src="https://github.com/user-attachments/assets/b87eb332-6a88-4b88-9769-24bb87330450" />

## Notes
- Subsurface Scattering with Analytic BSSRDFs
  - Please refer to my blog at: https://kai-z99.github.io/blog/Implementing-Subsurface-Scattering-with-Analytic-BSSRDFs.html

- Rough Dielectric BSDF, Conductor BSDF
   - https://www.pbr-book.org/4ed/Reflection_Models/Rough_Dielectric_BSDF
   - https://www.pbr-book.org/4ed/Reflection_Models/Conductor_BRDF
    
- BVH = Bounding Volume Hierarchy 
  - The BVH is built with a median-split strategy along the longest axis of each AABB.
  - This approach delivers very fast build times (≈O(n log n)), but is less optimized for ray-tracing cost. A binned Surface Area Heuristic (SAH) pass will improve traversal performance further.
  - https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/

- NEE (Next Event Estimation)
  - We can explicity sample direct lighting to reduce variance, especially on scenes where BSDF sampling would rarely hit a light.
  - Blog coming soon on this. 
  - Left: NEE with 110spp (~11 seconds)
  - Right: No NEE with 256spp (~11 seconds)
<p float="left">
  <img src="https://github.com/user-attachments/assets/55dd78fd-556f-4617-acec-613a41eacbbf" width="400" />
  <img src="https://github.com/user-attachments/assets/e12ecdbb-fa80-4fa8-8c0d-aaebf1eeb57a" width="400" /> 
</p>


    





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
<img width="1229" height="1079" alt="dragonGlass0 1-10000" src="https://github.com/user-attachments/assets/76edb716-df16-4925-b362-a49d671d86cb" />
<img width="1230" height="1107" alt="dragonJade10000spp" src="https://github.com/user-attachments/assets/b1654e77-7cc0-4ac3-865a-8ad2f451d393" />


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
<img width="1918" height="1110" alt="skylight2048spp-10 0" src="https://github.com/user-attachments/assets/0cf81441-3dbc-4222-8789-1b7946781423" />
<img width="1230" height="1110" alt="bunnySSS1" src="https://github.com/user-attachments/assets/3c664f4e-9a3a-4f1e-a4a2-946fa3ba9d10" />
<img width="1919" height="1077" alt="buddhaCompare15000" src="https://github.com/user-attachments/assets/82e00bf6-af81-48ef-8677-86153c92bf57" />









