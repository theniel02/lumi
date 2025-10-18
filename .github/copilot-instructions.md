The repository is a small C++ desktop viewer that combines ImGui (UI) with OpenCASCADE (CAD kernel) and GLFW/OpenGL for rendering.

Keep suggestions focused, practical, and specific to this codebase. Prefer small, incremental changes and reference the existing build system (CMake) and the single main app in `src/main.cpp`.

Key facts an agent should know:

- Build system: CMake (top-level `CMakeLists.txt`). On Windows the project expects vcpkg (or manual paths). README.md shows common commands:
  - Setup vcpkg, install `glfw3:x64-windows` and `opencascade:x64-windows`.
  - Generate and build with CMake; open `ImGuiOCCTApp.sln` for Visual Studio.

- Primary binary: `LUMI` built from `src/main.cpp`. ImGui sources live under `external/imgui` and are compiled into a static `imgui` target defined in CMake.

- Rendering and dependencies:
  - Uses legacy fixed-function OpenGL calls (gluLookAt, glBegin/glEnd) in `src/main.cpp`.
  - Requires GLU for `gluPerspective`/`gluLookAt` (handled in CMake). Be conservative when modernizing GL code.

- OpenCASCADE usage:
  - `src/main.cpp` demonstrates primary interactions: creating shapes (BRepPrimAPI_MakeBox), meshing (BRepMesh_IncrementalMesh), traversing faces (TopExp_Explorer), and calculating properties (BRepGProp/BRepBndLib).
  - Naming and API choices follow OpenCASCADE idioms: `TopoDS_Shape`, `TopoDS_Face`, `Handle(Poly_Triangulation)`, `TopLoc_Location`.

When editing code, prefer the repository's conventions:

- Keep C++17 and MSVC/GCC flags defined in CMake. Avoid changing global compiler flags unless necessary and justified in the patch description.
- Preserve use of ImGui as a bundled external dependency under `external/imgui`. If adding new ImGui code, add it under `external/imgui` or include via CMake.
- Prefer small, testable changes: adding a new UI control should be accompanied by a minimal, observable behavior change in `src/main.cpp`.

Common tasks and how to approach them:

- Build locally (Windows, recommended):
  1. Install vcpkg and run: `vcpkg install glfw3:x64-windows opencascade:x64-windows`
  2. Configure CMake with the vcpkg toolchain or set CMAKE_PREFIX_PATH as in `CMakeLists.txt`.
  3. Build via `cmake --build . --config Release` or open `ImGuiOCCTApp.sln` in Visual Studio.

- Add features that touch OpenCASCADE geometry: use `BRepPrimAPI_*` to create shapes, call `BRepMesh_IncrementalMesh` before extracting triangulation, then follow the existing pattern in `ExtractMesh` to populate `MeshData`.

- Debugging: runtime issues commonly stem from missing DLLs (Windows) or missing GLU/OpenCASCADE on Linux. Reproduce failures with the Visual Studio run target or a terminal build to capture linker/runtime messages.

Example idioms to follow (from `src/main.cpp`):

- Triangulation extraction: call `BRepMesh_IncrementalMesh mesh(shape, 0.1); mesh.Perform(); Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);` then iterate `tri->NbTriangles()` and `tri->Node(i)`.
- Face property computation: `GProp_GProps props; BRepGProp::SurfaceProperties(face, props); double area = props.Mass();`

When updating this file, preserve these constraints:

- Don't remove the bundled ImGui directory or switch to an external package without updating `CMakeLists.txt`.
- Avoid sweeping modernization of OpenGL to core profile unless also updating the CMake/OpenGL loader settings and testing on Windows.

If you need clarification while authoring changes, ask about:

- Target platform for the change (Windows vs Linux vs macOS).
- Whether to keep the fixed-function OpenGL renderer or migrate to modern OpenGL.

Files to inspect first for context: `CMakeLists.txt`, `README.md`, `src/main.cpp`, `external/imgui/*`.

End of file.
