// Harness nativo do overlay.
//
// Abre a MESMA janela ImGui que o mod desenha em jogo, com um simulador de
// combate no lugar do hook e um backend de áudio que só registra as chamadas.
// Serve para iterar a UI e a lógica no macOS ou no Linux, sem o jogo e sem
// Windows. Não faz parte do pacote distribuído.
//
//   xmake build harness && xmake run harness

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>

#include "Core/ModController.hpp"
#include "Support/Log.hpp"
#include "UI/Panels.hpp"
#include "UI/Theme.hpp"

namespace
{
constexpr const char* kSampleDir = "harness/sample";

// Painel que substitui o hook do jogo: escolhe um encontro e "inicia" o combate.
void draw_simulator(e33::ModController& mod, const e33::RecordingAudioBackend& backend,
                    int& selected_boss, int& selected_original)
{
    ImGui::SetNextWindowPos(ImVec2{800.0f, 40.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{440.0f, 420.0f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Simulador de combate (substitui o hook)");
    ImGui::TextWrapped("Em jogo, quem dispara isso e o hook de inicio de combate. Aqui o botao "
                       "chama o mesmo caminho: CombatWatcher::simulate().");
    ImGui::Separator();

    const auto& bosses = mod.catalog().bosses();
    const auto& tracks = mod.catalog().tracks();
    if (bosses.empty() || tracks.empty())
    {
        ImGui::TextColored(ImVec4{1.0f, 0.55f, 0.55f, 1.0f},
                           "Fixtures nao carregadas: rode a partir da raiz do repo.");
        ImGui::End();
        return;
    }

    selected_boss = std::min(selected_boss, static_cast<int>(bosses.size()) - 1);
    selected_original = std::min(selected_original, static_cast<int>(tracks.size()) - 1);

    if (ImGui::BeginCombo("Encontro", bosses[static_cast<std::size_t>(selected_boss)].name.c_str()))
    {
        for (int i = 0; i < static_cast<int>(bosses.size()); ++i)
        {
            if (ImGui::Selectable(bosses[static_cast<std::size_t>(i)].name.c_str(),
                                  i == selected_boss))
            {
                selected_boss = i;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginCombo("Faixa original",
                          tracks[static_cast<std::size_t>(selected_original)].name.c_str()))
    {
        for (int i = 0; i < static_cast<int>(tracks.size()); ++i)
        {
            if (ImGui::Selectable(tracks[static_cast<std::size_t>(i)].name.c_str(),
                                  i == selected_original))
            {
                selected_original = i;
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Iniciar combate"))
    {
        mod.watcher().simulate({bosses[static_cast<std::size_t>(selected_boss)].id,
                                tracks[static_cast<std::size_t>(selected_original)].id});
    }
    ImGui::SameLine();
    if (ImGui::Button("Encontro fora do catalogo"))
    {
        // Simula o que acontece depois de um patch: id novo, nao mapeado.
        mod.watcher().simulate({"Boss_DoPatchNovo",
                                tracks[static_cast<std::size_t>(selected_original)].id});
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Faixas que o backend recebeu:");
    if (backend.played().empty())
    {
        ImGui::TextDisabled("(nenhuma — nenhum override se aplicou)");
    }
    for (const auto& track : backend.played())
    {
        ImGui::BulletText("%s", track.c_str());
    }
    if (!backend.previewing().empty())
    {
        ImGui::TextColored(ImVec4{0.55f, 0.85f, 0.6f, 1.0f}, "preview: %s",
                           backend.previewing().c_str());
    }
    ImGui::End();
}
} // namespace

int main()
{
    if (glfwInit() == 0)
    {
        std::fprintf(stderr, "glfwInit falhou\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "Boss Music Swapper — harness", nullptr,
                                          nullptr);
    if (window == nullptr)
    {
        std::fprintf(stderr, "glfwCreateWindow falhou\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // Mesmo estilo e mesma fonte que o mod usa em jogo. O harness só serve
    // para julgar a aparência se for exatamente a mesma configuração.
    e33::ui::theme::apply_style();
    e33::ui::theme::load_fonts("assets");
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    e33::log::set_verbose(true);

    auto* backend = new e33::RecordingAudioBackend{};
    e33::ModController mod{std::unique_ptr<e33::IAudioBackend>{backend}};
    mod.initialize(std::filesystem::path{kSampleDir});

    e33::ui::OverlayState overlay;
    overlay.open = true; // no harness a janela ja abre; em jogo e a hotkey

    int selected_boss = 0;
    int selected_original = 0;

    while (glfwWindowShouldClose(window) == 0)
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        mod.tick(glfwGetTime());
        e33::ui::draw_overlay(mod, overlay);
        draw_simulator(mod, *backend, selected_boss, selected_original);

        if (!overlay.open)
        {
            // Em jogo isso e a hotkey; aqui um botao, para nao perder a janela.
            ImGui::Begin("Overlay fechado");
            if (ImGui::Button("Reabrir overlay (hotkey em jogo)"))
            {
                overlay.open = true;
            }
            ImGui::End();
        }

        ImGui::Render();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.03f, 0.028f, 0.025f, 1.0f); // obsidiana, como o fundo do jogo
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
