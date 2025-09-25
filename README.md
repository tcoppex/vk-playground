![language: c++20](https://img.shields.io/badge/c++-20-blue.svg)
![api: vulkan1.1](https://img.shields.io/badge/vulkan-1.1-red.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# :fire: vk-playground :fire:

A c++20 / Vulkan 1.1 rendering framework flavored like 1.4 and inspired by WebGPU and _vk_minimal_latest_.

Runs on GNU/Linux, Windows 11 and Android 32, compiled against _GCC 11.4_, _MSVC 19.38_, and _Clang 21_.

<details>
  <summary><strong>Quick start & run !</strong></summary>

```bash
# [Optional] Retrieve system build dependencies with Synaptic.
# sudo apt install git git-lfs build-essential cmake vulkan-sdk

# [Optionnal] Specify the ANDROID_SDK path to create Android targets.
# export ANDROID_SDK=~/Android/Sdk

# Clone the repository.
git clone https://github.com/tcoppex/vk-playground
cd vk-playground

# Build.
cmake . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run the first sample.
./bin/00_hello

# [Optionnal] Build & Run an Android sample on a connected device.
# cmake --build build --target run_aloha
```

</details>


### Demos

* **[00_hello](samples/desktop/00_hello)**: Display a surface and clear its color (_Device, Swapchain, dynamic rendering_).
* **[01_triangle](samples/desktop/01_triangle)**: Display a simple triangle (_Shader, Graphics Pipeline, Vertex Buffer, Commands_).
* **[02_push_constant](samples/desktop/02_push_constant)**: Update per-frame values via push constants and dynamic states (_Push Constant_).
* **[03_descriptor_set](samples/desktop/03_descriptor_set)**: Initialize & update a descriptor set on a single uniform buffer (_Descriptor Set_).
* **[04_texturing](samples/desktop/04_texturing)**: Display a textured cube with a linear sampler (_Image, Sampler_).
* **[05_stencil_op](samples/desktop/05_stencil_op)**: Stencil operations and instancing through a multi-passes portal effect (_Stencil, instancing_).
* **[06_blend_op](samples/desktop/06_blend_op)**: Fast & simple billboarded GPU particles with additive blending (_Blending_).
* **[07_compute](samples/desktop/07_compute)**: Waves simulation with sorted alpha-blended particles (_Compute Pipeline, Buffer Barriers_).
* **[08_hdr_envmap](samples/desktop/08_hdr_envmap)**: Image-based lighting from a prefiltered HDR environment map (_Texture Barriers_).
* **[09_post_process](samples/desktop/09_post_process)**: Screen-space contour effect via a post-processing pipeline (_Render Target_, _Blit_).
* **[10_material](samples/desktop/10_material)**: Showcase the internal PBR material system with scene graph ordering (_Pipeline Cache_, _Specialization Constants_).
* **[11_raytracing](samples/desktop/11_raytracing)**: Simple path tracer on a Cornell box via hardware-accelerated ray tracing (_Acceleration Structure_, _Ray Tracing Pipeline_, _Buffer Device Address_).

### Dependencies

##### Third parties

 * Vulkan SDK 1.1
 * CMake 3.22.1
 * CPM 0.40.3 (_downloaded automatically_)
 * Volk 1.4 (_via CPM_)
 * VulkanMemoryAllocator 3.2.0 (_via CPM_)
 * GLFW 3.4 (_via CPM_)
 * ImGUI (_via CPM_)
 * MikkTSpace (_via CPM_)
 * linalg v2.2 (_via CPM_)
 * libfmt 12.0 (_via CPM_)
 * stb_image.h (_included_)

##### Vulkan device extensions

* [VK_EXT_descriptor_indexing](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_descriptor_indexing.html)
* [VK_EXT_extended_dynamic_state](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_extended_dynamic_state.html)
* [VK_EXT_extended_dynamic_state2](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_extended_dynamic_state2.html)
* [VK_EXT_extended_dynamic_state3](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_extended_dynamic_state3.html)
* [VK_EXT_image_view_min_lod](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_image_view_min_lod.html)
* [VK_EXT_vertex_input_dynamic_state](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_vertex_input_dynamic_state.html)
* [VK_KHR_buffer_device_address]( https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_buffer_device_address.html)
* [VK_KHR_create_renderpass2](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_create_renderpass2.html)
* [VK_KHR_depth_stencil_resolve](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_depth_stencil_resolve.html)
* [VK_KHR_dynamic_rendering](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_dynamic_rendering.html)
* [VK_KHR_index_type_uint8](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_index_type_uint8.html)
* [VK_KHR_maintenance4](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_maintenance4.html)
* [VK_KHR_maintenance5](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_maintenance5.html)
* [VK_KHR_maintenance6](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_maintenance6.html) (_optionnal_)
* [VK_KHR_swapchain](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_swapchain.html)
* [VK_KHR_synchronization2](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_synchronization2.html)
* [VK_KHR_timeline_semaphore](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_timeline_semaphore.html)
* [VK_KHR_acceleration_structure](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_acceleration_structure.html)
* [VK_KHR_ray_tracing_pipeline](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_ray_tracing_pipeline.html) (_optionnal_)

##### Android build

 * Android SDK 36
 * Android NDK 29.0.14033849
 * Gradle 8.14.3

###### Installing Android dependencies

<details>
  <summary><strong>for GNU/Linux</strong></summary>

```bash
# Install tools and the JDK
sudo apt-get install -y unzip wget openjdk-17-jdk

# Setup ANDROID_SDK
export ANDROID_SDK=$HOME/Android
mkdir $ANDROID_SDK && cd $ANDROID_SDK

# Download & install Android SDK Command-line Tools 12.0.
wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
unzip commandlinetools-linux-11076708_latest.zip -d cmdline-tools
mv cmdline-tools/cmdline-tools cmdline-tools/latest
export PATH=$ANDROID_SDK/cmdline-tools/latest/bin:$ANDROID_SDK/platform-tools:$PATH

# Install dependencies.
sdkmanager "platforms;android-36" "platform-tools" "build-tools;36.0.0" "ndk;29.0.14033849"
```
</details>

<details>
  <summary><strong>for Windows 11</strong></summary>

```bash
# Install JDK manually on Windows (eg. Temurin 17)

# Setup ANDROID_SDK
export ANDROID_SDK=$HOME/Android
mkdir -p $ANDROID_SDK && cd $ANDROID_SDK

# Download & install Android SDK Command-line Tools 12.0.
curl -O https://dl.google.com/android/repository/commandlinetools-win-11076708_latest.zip
unzip commandlinetools-win-11076708_latest.zip -d cmdline-tools
mv cmdline-tools/cmdline-tools cmdline-tools/latest
export PATH=$ANDROID_SDK/cmdline-tools/latest/bin:$ANDROID_SDK/platform-tools:$PATH

# Install dependencies.
sdkmanager "platforms;android-36" "platform-tools" "build-tools;36.0.0" "ndk;29.0.14033849"
```
</details>

<br/>

###### Build and Run

For each Android sample a collection of CMake debug targets using the template `{prefix}{sample_name}`
are availables to simplify development without the need to use Android Studio :

| Prefix            | Description | 
|-------------------|-------------|
| **build_**        | build the target in debug mode |
| **install_**      | build and install the target   |
| **run_**          | build, install, and run the target |
| **log_**          | build, install, run and log the target outputs |

_On device targets (eg. install, run, log) require a compatible connected device to work._

#### Assets

A few assets are served via `git-lfs` but most are downloaded automatically
as needed on CMake cache generation time.

### Acknowledgement

This project was inspired by the work of **NVIDIA DesignWorks Samples**, in particular the _[vk_minimal_latest](https://github.com/nvpro-samples/vk_minimal_latest)_ project.

### License

*vk-playground* is released under the *MIT* license.
