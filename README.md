# tinyrenderer

A small renderer for learning graphics APIs and rendering techniques.

## Features

- PBR materials
- OBJ, glTF and FBX loading
- Directional light with shadow mapping
- MSAA
- Free and orbit cameras
- ImGui controls

## Dependencies

- CMake 4.3.1+
- MSVC (C++20)
- Vulkan SDK 1.3+
- Slang compiler (`slangc`) in `PATH` (most of the time provided with Vulkan SDK)
- vcpkg with `VCPKG_ROOT` set
- ufbx (included in the repository)

Installed through [vcpkg.json](vcpkg.json) manifest:

- GLFW
- GLM
- ImGui (Vulkan backend)
- fmt
- stb
- rapidobj
- fastgltf
- tinyexr

## Build

Debug:

```sh
cmake --preset windows-debug
cmake --build --preset win-debug
```

Release:

```sh
cmake --preset windows-release
cmake --build --preset win-release
```

Executables are written to `build/win-debug` and `build/win-release`.

## Controls

- Hold right mouse button to look around
- `WASD` to move
- `Q` / `E` to move down and up
- Hold `Shift` to move faster
- `F3` to toggle the UI

## Conventions

- Counter-Clockwise winding
- Left-handed coordinate system

## Used Resources

- [Vulkan Tutorial](https://docs.vulkan.org/tutorial/latest/) – an ok basics with weird learning curve a the start.
- [Learn OpenGL](https://learnopengl.com/) – an excellent and clear overview of various techniques that can be applied not only in OpenGL.
- [Art of Render Graphs](https://alielmorsy.github.io/the-art-of-render-graphs/) - good starting point for understanding the render graph system.

## TODO

- **Porting**
  - [ ] Linux
  - [ ] Web
  - [ ] Android
- **Rendering Backends**
  - [ ] WebGPU
  - [ ] DirectX
  - [ ] OpenGL
- **Rendering Techniques**
  - [x] Skybox Import
  - [x] Render Graph
  - [x] Image-based lighting
  - [ ] Instanced Rendering
  - [ ] Procedural default skybox
  - [ ] Cel-Shading
  - [ ] Surface Dithering
  - [ ] Occlusion Culling
  - [ ] Point and spot lights
  - [ ] Imported scene lights
  - [ ] Animations
  - [ ] Cascaded shadow maps
  - [ ] GPU Skinning
  - [ ] GPU-driven rendering
  - [ ] OIT
  - [ ] Ray Tracing
- **Post-processing**
  - [x] HDR and Tonemapping
  - [ ] Ambient Occlusion
  - [ ] Color grading
  - [ ] Vignette
  - [ ] Film grain
  - [ ] Chromatic aberration
  - [ ] Bloom
  - [ ] Outline
  - [ ] ASCII
  - [ ] 3D Pixel Art
  - [ ] Depth of field
  - [ ] Motion blur
  - [ ] Fog
  - [ ] Custom runtime fullscreen effects
- **Utility**
  - [ ] Screenshot Export
  - [ ] Drag-and-drop model loading
  - [ ] File Associations
  - [ ] Recent files
  - [ ] GPU and CPU profiling
  - [ ] FPS and frame-time graph
  - [ ] VRAM and resource usage display
  - [ ] Debug views of internal buffers and images
  - [ ] Scene hierarchy
