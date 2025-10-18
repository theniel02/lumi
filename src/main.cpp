

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"




#include <stdio.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

// OpenCASCADE includes
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <GProp_GProps.hxx>
#include <BRepGProp.hxx>
#include <Poly_Triangulation.hxx>
#include <BRep_Tool.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>

#include <vector>
#include <string>
#include <cmath>

#include "font_data.h"


// Estrutura para armazenar a malha triangulada
struct MeshData {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<unsigned int> indices;
    gp_Pnt center;
    double boundingRadius;
    
    void clear() {
        vertices.clear();
        normals.clear();
        indices.clear();
    }
};

// Estrutura para picking de faces
struct FaceData {
    int faceIndex;
    double area;
    gp_Pnt center;
};

// Estado da aplicação
struct AppState {
    // Dimensões do box
    float width = 10.0f;
    float height = 20.0f;
    float depth = 30.0f;
    
    // Controle de câmera
    float cameraDistance = 100.0f;
    float cameraAngleX = 30.0f;
    float cameraAngleY = 45.0f;
    bool isDragging = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    
    // Face selecionada
    int selectedFace = -1;
    double selectedFaceArea = 0.0;
    
    // Malha do objeto
    MeshData mesh;
    TopoDS_Shape currentShape;
    std::vector<FaceData> faces;
};

static AppState g_appState;

// Função para calcular a normal de um triângulo
gp_Vec CalculateNormal(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& p3) {
    gp_Vec v1(p1, p2);
    gp_Vec v2(p1, p3);
    gp_Vec normal = v1.Crossed(v2);
    
    if (normal.Magnitude() > 1e-10) {
        normal.Normalize();
    }
    
    return normal;
}

// Função para triangular e extrair malha do shape
void ExtractMesh(const TopoDS_Shape& shape, MeshData& meshData) {
    meshData.clear();
    g_appState.faces.clear();
    
    // Triangular o shape
    BRepMesh_IncrementalMesh mesh(shape, 0.1);
    mesh.Perform();
    
    if (!mesh.IsDone()) {
        printf("Erro na triangulação\n");
        return;
    }
    
    unsigned int globalVertexOffset = 0;
    int faceIndex = 0;
    
    // Iterar sobre todas as faces
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next(), faceIndex++) {
        TopoDS_Face face = TopoDS::Face(exp.Current());
        TopLoc_Location location;
        
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        
        if (triangulation.IsNull()) continue;
        
        // Calcular área da face
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        double faceArea = props.Mass();
        
        // Centro da face
        gp_Pnt faceCenter = props.CentreOfMass();
        
        // Armazenar dados da face
        FaceData faceData;
        faceData.faceIndex = faceIndex;
        faceData.area = faceArea;
        faceData.center = faceCenter;
        g_appState.faces.push_back(faceData);
        
        // Use modern Poly_Triangulation accessors
        int nbNodes = triangulation->NbNodes();
        int nbTriangles = triangulation->NbTriangles();

        // Armazenar vértices
        unsigned int vertexOffset = meshData.vertices.size() / 3;

        for (int i = 1; i <= nbNodes; ++i) {
            gp_Pnt p = triangulation->Node(i).Transformed(location.Transformation());
            meshData.vertices.push_back((float)p.X());
            meshData.vertices.push_back((float)p.Y());
            meshData.vertices.push_back((float)p.Z());
        }

        // Processar triângulos
        for (int i = 1; i <= nbTriangles; ++i) {
            int n1, n2, n3;
            triangulation->Triangle(i).Get(n1, n2, n3);

            // Ajustar índices from 1-based Poly indices to 0-based vertex array plus offset
            n1 = n1 - 1 + vertexOffset;
            n2 = n2 - 1 + vertexOffset;
            n3 = n3 - 1 + vertexOffset;
            
            meshData.indices.push_back(n1);
            meshData.indices.push_back(n2);
            meshData.indices.push_back(n3);
            
            // Calcular normal
            gp_Pnt p1(meshData.vertices[n1*3], meshData.vertices[n1*3+1], meshData.vertices[n1*3+2]);
            gp_Pnt p2(meshData.vertices[n2*3], meshData.vertices[n2*3+1], meshData.vertices[n2*3+2]);
            gp_Pnt p3(meshData.vertices[n3*3], meshData.vertices[n3*3+1], meshData.vertices[n3*3+2]);
            
            gp_Vec normal = CalculateNormal(p1, p2, p3);
            
            // Adicionar normal para cada vértice do triângulo
            for (int j = 0; j < 3; j++) {
                meshData.normals.push_back((float)normal.X());
                meshData.normals.push_back((float)normal.Y());
                meshData.normals.push_back((float)normal.Z());
            }
        }
    }
    
    // Calcular centro e raio do bounding box
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    
    if (!bbox.IsVoid()) {
        double xmin, ymin, zmin, xmax, ymax, zmax;
        bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        
        meshData.center.SetX((xmin + xmax) / 2.0);
        meshData.center.SetY((ymin + ymax) / 2.0);
        meshData.center.SetZ((zmin + zmax) / 2.0);
        
        double dx = xmax - xmin;
        double dy = ymax - ymin;
        double dz = zmax - zmin;
        meshData.boundingRadius = sqrt(dx*dx + dy*dy + dz*dz) / 2.0;
    }
    
    printf("Malha extraída: %zu vértices, %zu triângulos, %zu faces\n", 
           meshData.vertices.size()/3, meshData.indices.size()/3, g_appState.faces.size());
}

// Função para criar o box
void CreateBox(float width, float height, float depth) {
    try {
        BRepPrimAPI_MakeBox boxMaker(width, height, depth);
        g_appState.currentShape = boxMaker.Shape();
        ExtractMesh(g_appState.currentShape, g_appState.mesh);
        g_appState.selectedFace = -1;
        g_appState.selectedFaceArea = 0.0;
    } catch (const std::exception& e) {
        printf("Erro ao criar box: %s\n", e.what());
    }
}

// Função para renderizar a malha com OpenGL
void RenderMesh(const MeshData& mesh) {
    if (mesh.vertices.empty() || mesh.indices.empty()) return;
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Configurar câmera
    float camX = g_appState.cameraDistance * sin(g_appState.cameraAngleY * 3.14159f / 180.0f) * cos(g_appState.cameraAngleX * 3.14159f / 180.0f);
    float camY = g_appState.cameraDistance * sin(g_appState.cameraAngleX * 3.14159f / 180.0f);
    float camZ = g_appState.cameraDistance * cos(g_appState.cameraAngleY * 3.14159f / 180.0f) * cos(g_appState.cameraAngleX * 3.14159f / 180.0f);
    
    gluLookAt(camX + mesh.center.X(), camY + mesh.center.Y(), camZ + mesh.center.Z(),
              mesh.center.X(), mesh.center.Y(), mesh.center.Z(),
              0.0, 1.0, 0.0);
    
    // Iluminação
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);
    
    GLfloat lightPos[] = { 1.0f, 1.0f, 1.0f, 0.0f };
    GLfloat lightAmbient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat lightDiffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    
    // Desenhar malha
    glColor3f(0.6f, 0.7f, 0.9f);
    
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < mesh.indices.size(); i++) {
        unsigned int idx = mesh.indices[i];
        
        if (mesh.normals.size() > i * 3 + 2) {
            glNormal3f(mesh.normals[i*3], mesh.normals[i*3+1], mesh.normals[i*3+2]);
        }
        
        if (mesh.vertices.size() > idx * 3 + 2) {
            glVertex3f(mesh.vertices[idx*3], mesh.vertices[idx*3+1], mesh.vertices[idx*3+2]);
        }
    }
    glEnd();
    
    // Desenhar wireframe
    glDisable(GL_LIGHTING);
    glColor3f(0.2f, 0.2f, 0.2f);
    glLineWidth(1.0f);
    
    glBegin(GL_LINES);
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        for (int j = 0; j < 3; j++) {
            unsigned int idx1 = mesh.indices[i + j];
            unsigned int idx2 = mesh.indices[i + ((j + 1) % 3)];
            
            glVertex3f(mesh.vertices[idx1*3], mesh.vertices[idx1*3+1], mesh.vertices[idx1*3+2]);
            glVertex3f(mesh.vertices[idx2*3], mesh.vertices[idx2*3+1], mesh.vertices[idx2*3+2]);
        }
    }
    glEnd();
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1600, 900, "LUMI - ", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImFont* font = io.Fonts->AddFontFromMemoryTTF(
		(void*)MPLUSRounded1c_Medium_ttf,
		MPLUSRounded1c_Medium_ttf_len,
		18.0f
	);


    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // ======= BASE DO TEMA =======
	ImGui::StyleColorsDark(); // base escura

	
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// ======= JANELAS =======
	colors[ImGuiCol_WindowBg]           = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);   // fundo da janela (cinza escuro)
	colors[ImGuiCol_ChildBg]            = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);   // painéis internos
	colors[ImGuiCol_PopupBg]            = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);   // menus e popups
	colors[ImGuiCol_Border]             = ImVec4(0.25f, 0.25f, 0.25f, 0.7f);   // bordas sutis

	// ======= TÍTULOS E BARRAS =======
	colors[ImGuiCol_TitleBg]            = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
	colors[ImGuiCol_TitleBgActive]      = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
	colors[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.10f, 0.10f, 0.10f, 0.8f);

	// ======= ABAS =======
	colors[ImGuiCol_Tab]                = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
	colors[ImGuiCol_TabHovered]         = ImVec4(0.25f, 0.40f, 0.65f, 1.0f);
	colors[ImGuiCol_TabActive]          = ImVec4(0.22f, 0.50f, 0.80f, 1.0f);
	colors[ImGuiCol_TabUnfocused]       = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

	// ======= BOTÕES =======
	colors[ImGuiCol_Button]             = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
	colors[ImGuiCol_ButtonHovered]      = ImVec4(0.35f, 0.55f, 0.80f, 1.0f);
	colors[ImGuiCol_ButtonActive]       = ImVec4(0.15f, 0.35f, 0.65f, 1.0f);

	// ======= TEXTO =======
	colors[ImGuiCol_Text]               = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
	colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);

	// ======= FRAMES E CAMPOS =======
	colors[ImGuiCol_FrameBg]            = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
	colors[ImGuiCol_FrameBgHovered]   	= ImVec4(0.25f, 0.25f, 0.30f, 1.0f);
	colors[ImGuiCol_FrameBgActive]      = ImVec4(0.20f, 0.40f, 0.70f, 1.0f);

	// ======= SCROLLBAR =======
	colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
	colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabHovered]=ImVec4(0.35f, 0.55f, 0.85f, 1.0f);
	colors[ImGuiCol_ScrollbarGrabActive]= ImVec4(0.30f, 0.50f, 0.80f, 1.0f);

	// ======= SLIDERS =======
	colors[ImGuiCol_SliderGrab]       = ImVec4(0.35f, 0.55f, 0.90f, 1.0f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.65f, 1.00f, 1.0f);
	colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.25f, 0.25f, 0.30f, 1.0f);

	// ======= SEPARADORES =======
	colors[ImGuiCol_Separator]          = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
	colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.45f, 0.60f, 0.80f, 1.0f);
	colors[ImGuiCol_SeparatorActive]    = ImVec4(0.25f, 0.45f, 0.80f, 1.0f);

	// ======= ESTILO =======
	style.WindowRounding = 5.0f;
	style.FrameRounding  = 5.0f;
	style.TabRounding    = 1.0f;
	style.GrabRounding   = 3.0f;
	style.ScrollbarRounding = 5.0f;
	style.FrameBorderSize = 1.0f;
	style.WindowBorderSize = 1.0f;
	style.ItemSpacing = ImVec2(8, 6);
	style.WindowPadding = ImVec2(10, 8);

    // Initialize ImGui GLFW backend but do not install callbacks so we can forward events
    // from our own handlers (allows respecting io.WantCaptureMouse / keyboard)
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Criar box inicial
    CreateBox(g_appState.width, g_appState.height, g_appState.depth);

    // Mouse callback para rotação da câmera — forward events to ImGui first
    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        // Let ImGui process the event first
        ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);

		
        ImGuiIO& io = ImGui::GetIO();
		ImFont* font = io.Fonts->AddFontFromMemoryTTF(
			(void*)MPLUSRounded1c_Medium_ttf,
			MPLUSRounded1c_Medium_ttf_len,
			18.0f
		);

        if (io.WantCaptureMouse) {
            // If ImGui wants the mouse, don't process it in the app
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                g_appState.isDragging = true;
                glfwGetCursorPos(win, &g_appState.lastMouseX, &g_appState.lastMouseY);
            } else if (action == GLFW_RELEASE) {
                g_appState.isDragging = false;
            }
        }
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
        // Forward to ImGui
        ImGui_ImplGlfw_CursorPosCallback(win, xpos, ypos);

        ImGuiIO& io = ImGui::GetIO();
		ImFont* font = io.Fonts->AddFontFromMemoryTTF(
			(void*)MPLUSRounded1c_Medium_ttf,
			MPLUSRounded1c_Medium_ttf_len,
			18.0f
		);
        if (io.WantCaptureMouse) return;

        if (g_appState.isDragging) {
            double dx = xpos - g_appState.lastMouseX;
            double dy = ypos - g_appState.lastMouseY;

            g_appState.cameraAngleY += (float)dx * 0.5f;
            g_appState.cameraAngleX += (float)dy * 0.5f;

            // Limitar ângulo vertical
            if (g_appState.cameraAngleX > 89.0f) g_appState.cameraAngleX = 89.0f;
            if (g_appState.cameraAngleX < -89.0f) g_appState.cameraAngleX = -89.0f;

            g_appState.lastMouseX = xpos;
            g_appState.lastMouseY = ypos;
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
        // Forward to ImGui
        ImGui_ImplGlfw_ScrollCallback(win, xoffset, yoffset);

        ImGuiIO& io = ImGui::GetIO();
		ImFont* font = io.Fonts->AddFontFromMemoryTTF(
			(void*)MPLUSRounded1c_Medium_ttf,
			MPLUSRounded1c_Medium_ttf_len,
			18.0f
		);
        if (io.WantCaptureMouse) return;

        g_appState.cameraDistance -= (float)yoffset * 5.0f;
        if (g_appState.cameraDistance < 10.0f) g_appState.cameraDistance = 10.0f;
        if (g_appState.cameraDistance > 500.0f) g_appState.cameraDistance = 500.0f;
    });

    ImVec4 clear_color = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Dockspace
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                       ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);
        
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();

        // Janela de controle de dimensões
        ImGui::Begin("Dimensões do Retângulo", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        ImGui::Text("Ajuste as dimensões:");
        ImGui::Separator();
        
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Largura (X)", &g_appState.width, 1.0f, 100.0f, "%.1f");
        ImGui::SliderFloat("Altura (Y)", &g_appState.height, 1.0f, 100.0f, "%.1f");
        ImGui::SliderFloat("Profundidade (Z)", &g_appState.depth, 1.0f, 100.0f, "%.1f");
        ImGui::PopItemWidth();
        
        ImGui::Separator();
        
        if (ImGui::Button("Aplicar Dimensões", ImVec2(200, 40))) {
            CreateBox(g_appState.width, g_appState.height, g_appState.depth);
        }
        
        ImGui::End();

        // Janela de informações da face selecionada
        ImGui::Begin("Informações da Face", nullptr);
        
        if (g_appState.selectedFace >= 0) {
            ImGui::Text("Face Selecionada: %d", g_appState.selectedFace + 1);
            ImGui::Text("Área: %.2f unidades²", g_appState.selectedFaceArea);
            ImGui::Separator();
            
            if (ImGui::Button("Limpar Seleção")) {
                g_appState.selectedFace = -1;
                g_appState.selectedFaceArea = 0.0;
            }
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Nenhuma face selecionada");
            ImGui::Separator();
            ImGui::TextWrapped("Clique com botão esquerdo em uma face para ver sua área");
        }
        
        ImGui::End();

        // Janela de visualização 3D
        ImGui::Begin("Visualização 3D");
        ImGui::Text("Controles:");
        ImGui::BulletText("Botão direito: rotacionar câmera");
        ImGui::BulletText("Scroll: zoom");
        ImGui::BulletText("Botão esquerdo: selecionar face");
        ImGui::Separator();
        
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Configurar projeção
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)display_w / (double)display_h, 0.1, 1000.0);
        
        // Renderizar malha
        RenderMesh(g_appState.mesh);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}