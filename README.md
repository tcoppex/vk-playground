![language: c++20](https://img.shields.io/badge/c++-20-blue.svg)
![api: vulkan1.1](https://img.shields.io/badge/vulkan-1.1-red.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# :fire: vk-playground :fire:

A simple c++20 / Vulkan 1.1 rendering framework, flavored like 1.4, inspired by WebGPU and _vk_minimal_latest_.

Runs on GNU/Linux and Windows 11, compiled with _GCC 11.4_ and _MSVC 19.38_.

<details>
  <summary><strong>Quick start & run !</strong></summary>

```bash
# [Optional] Retrieve system build dependencies with Synaptic.
# sudo apt install git git-lfs build-essential cmake vulkan-sdk

# Clone the repository.
git clone https://github.com/tcoppex/vk-playground
cd vk-playground

# Build.
cmake . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run the first demo.
./bin/00_hello
```

</details>


<!-- 
#### Some Features

 * Swapchain management
 * Timeline semaphore
 * Legacy & Dynamic rendering
 * Vulkan Memory Allocator (VMA)
 * GLSL to SPIR-V compilation via CMake
 -->

### Demos

* **[00_hello](src/samples/00_hello)**: Display a surface and clear its color (_Device, Swapchain, dynamic rendering_).
* **[01_triangle](src/samples/01_triangle)**: Display a simple triangle (_Shader, Graphics Pipeline, Vertex Buffer, Commands_).
* **[02_push_constant](src/samples/02_push_constant)**: Update per-frame values via push constants and dynamic states (_Push Constant_).
* **[03_descriptor_set](src/samples/03_descriptor_set)**: Initialize & update a descriptor set on a single uniform buffer (_Descriptor Set_).
* **[04_texturing](src/samples/04_texturing)**: Display a textured cube with a linear sampler (_Image, Sampler_).
* **[05_stencil_op](src/samples/05_stencil_op)**: Demonstrate instancing and stencil operations through a multi-passes portal effect (_Stencil, instancing_).
* **[06_blend_op](src/samples/06_blend_op)**: Fast & simple billboarded GPU particles with additive blend operation (_Blending_).
* **[07_compute](src/samples/07_compute)**: Waves simulation with sorted alpha-blended particles via compute shaders (_Compute Pipeline, Barrier_).

### Dependencies

##### Third parties

 * Vulkan SDK 1.1
 * CMake 3.31
 * CPM 0.40.3 (_downloaded automatically_)
 * GLFW 3.4 (_via CPM_)
 * Volk 1.4 (_via CPM_)
 * VulkanMemoryAllocator 3.2.0 (_via CPM_)
 * linalg v2.2 (_via CPM_)
 * stb_image.h (_included_)

##### Vulkan device extensions

* [VK_EXT_descriptor_indexing](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_descriptor_indexing.html)
* [VK_EXT_extended_dynamic_state](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_extended_dynamic_state.html)
* [VK_EXT_extended_dynamic_state2](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_extended_dynamic_state2.html)
* [VK_EXT_extended_dynamic_state3](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_extended_dynamic_state3.html)
* [VK_KHR_buffer_device_address]( https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_buffer_device_address.html)
* [VK_KHR_create_renderpass2](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_create_renderpass2.html)
* [VK_KHR_depth_stencil_resolve](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_depth_stencil_resolve.html)
* [VK_KHR_dynamic_rendering](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_dynamic_rendering.html)
* [VK_KHR_maintenance4](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_maintenance4.html)
* [VK_KHR_maintenance5](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_maintenance5.html)
* [VK_KHR_maintenance6](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_maintenance6.html)
* [VK_KHR_swapchain](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_swapchain.html)
* [VK_KHR_synchronization2](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_synchronization2.html)
* [VK_KHR_timeline_semaphore](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_timeline_semaphore.html)

### License

*vk-playground* is released under the *MIT* license.