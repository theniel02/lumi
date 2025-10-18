cmake_minimum_required(VERSION 3.15)
project(LUMI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands (helps editors / clangd / IntelliSense find include paths)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configurações específicas por plataforma
if(WIN32)
    # Windows: usar vcpkg ou paths manuais
    # Se usar vcpkg, comente as linhas abaixo
    set(CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows" ${CMAKE_PREFIX_PATH})
    
    # Se NÃO usar vcpkg, descomente e ajuste os paths:
    # set(GLFW_DIR "C:/libs/glfw-3.4/lib/cmake/glfw3")
    # set(OpenCASCADE_DIR "C:/OpenCASCADE-7.7.0/cmake")
    
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
elseif(APPLE)
    # macOS: usar Homebrew paths
    set(CMAKE_PREFIX_PATH "/usr/local" "/opt/homebrew" ${CMAKE_PREFIX_PATH})
elseif(UNIX)
    # Linux: paths padrão do sistema
    set(CMAKE_PREFIX_PATH "/usr" "/usr/local" ${CMAKE_PREFIX_PATH})
endif()

# ==============================================================================
# Encontrar dependências
# ==============================================================================

# OpenGL (com GLU para gluLookAt e gluPerspective)
find_package(OpenGL REQUIRED)

# No Windows, GLU já vem com OpenGL
# No Linux/Mac, pode precisar instalar separadamente
if(NOT OPENGL_FOUND)
    message(FATAL_ERROR "OpenGL not found")
endif()

if(WIN32)
    # Windows: GLU é parte do OpenGL32
    message(STATUS "Windows: Using built-in GLU")
elseif(APPLE)
    # macOS: GLU está no framework OpenGL
    message(STATUS "macOS: Using framework GLU")
else()
    # Linux: verificar GLU
    if(NOT OPENGL_GLU_FOUND)
        message(WARNING "GLU not found, trying to find it manually")
        find_library(GLU_LIBRARY GLU)
        if(GLU_LIBRARY)
            message(STATUS "GLU found: ${GLU_LIBRARY}")
        else()
            message(FATAL_ERROR "GLU library not found. Install with: sudo apt install libglu1-mesa-dev")
        endif()
    endif()
endif()

# GLFW
find_package(glfw3 REQUIRED)

# OpenCASCADE
find_package(OpenCASCADE REQUIRED)
if(OpenCASCADE_FOUND)
    message(STATUS "OpenCASCADE found: ${OpenCASCADE_DIR}")
    include_directories(${OpenCASCADE_INCLUDE_DIR})
endif()

# ==============================================================================
# ImGui - Como submodule ou diretório local
# ==============================================================================

# Opção 1: ImGui como submodule (recomendado)
# git submodule add https://github.com/ocornut/imgui.git external/imgui

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

# Opção 2: Se ImGui estiver em outro lugar, ajuste o path:
# set(IMGUI_DIR "C:/libs/imgui")

if(NOT EXISTS ${IMGUI_DIR})
    message(FATAL_ERROR 
        "ImGui not found at ${IMGUI_DIR}\n"
        "Please either:\n"
        "  1. Clone ImGui: git clone https://github.com/ocornut/imgui.git external/imgui\n"
        "  2. Or set IMGUI_DIR to your ImGui installation path"
    )
endif()

# Arquivos do ImGui
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(IMGUI_HEADERS
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_internal.h
    ${IMGUI_DIR}/imconfig.h
    ${IMGUI_DIR}/backends/imgui_impl_glfw.h
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.h
)

# ==============================================================================
# Criar biblioteca ImGui estática
# ==============================================================================

add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    glfw
    OpenGL::GL
)

# Configurações específicas para Windows
if(WIN32)
    target_compile_definitions(imgui PRIVATE
        IMGUI_IMPL_OPENGL_LOADER_GLAD
    )
endif()

# ==============================================================================
# Executável principal
# ==============================================================================

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    inc
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${OpenCASCADE_INCLUDE_DIR}
    ${OpenCASCADE_INCLUDE_DIRS}
    ${OpenCASCADE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    glfw
    OpenGL::GL
    OpenGL::GLU
    ${OpenCASCADE_LIBRARIES}
)

# Bibliotecas OpenCASCADE específicas necessárias
target_link_libraries(${PROJECT_NAME} PRIVATE
    TKernel
    TKMath
    TKBRep
    TKPrim
    TKTopAlgo
    TKGeomBase
    TKG3d
    TKGeomAlgo
    TKBO
    TKMesh
)

# ==============================================================================
# Configurações específicas por plataforma
# ==============================================================================

if(WIN32)
    # Windows: copiar DLLs necessárias para o diretório de saída
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:glfw>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    
    # Definir como aplicação Windows (sem console)
    # Comente a linha abaixo se quiser ver o console
    # set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    
elseif(APPLE)
    # macOS: configurações específicas
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.yourcompany.imguiocct"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
    
    # Frameworks necessários no macOS
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
    
elseif(UNIX)
    # Linux: bibliotecas adicionais
    target_link_libraries(${PROJECT_NAME} PRIVATE
        pthread
        dl
    )
    
    # Adicionar X11 se necessário
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
endif()

# ==============================================================================
# Opções de compilação
# ==============================================================================

if(MSVC)
    # Visual Studio
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    )
else()
    # GCC/Clang
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# ==============================================================================
# Informações de build
# ==============================================================================

message(STATUS "==================================================")
message(STATUS "  ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "==================================================")

# Helper: copy compile_commands.json to repo root after configure so editors like vscode/clangd can pick it up
if(EXISTS "${CMAKE_BINARY_DIR}/compile_commands.json")
    file(COPY "${CMAKE_BINARY_DIR}/compile_commands.json" DESTINATION "${CMAKE_SOURCE_DIR}")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "==================================================")
message(STATUS "Dependencies:")
message(STATUS "  OpenGL: ${OPENGL_LIBRARIES}")
message(STATUS "  GLFW: Found")
message(STATUS "  ImGui: ${IMGUI_DIR}")
message(STATUS "  OpenCASCADE: ${OpenCASCADE_DIR}")
message(STATUS "==================================================")

# ==============================================================================
# Install (opcional)
# ==============================================================================

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)