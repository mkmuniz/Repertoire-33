#pragma once

#include <filesystem>
#include <functional>

// Integração com o jogo sem UE4SS.
//
// O UE4SS resolveria isto, mas compilar um mod C++ dele exige o repositório
// privado Re-UE4SS/UEPseudo (ver docs/BUILD-BLOCKER.md). A alternativa é
// pública inteira: a DLL entra por proxy, engancha o Present da swapchain e
// desenha o ImGui no quadro do jogo.
//
// Esta é a ÚNICA parte do mod que conhece Windows e Direct3D. Quando um patch
// do jogo ou um driver quebrar o overlay, o estrago está aqui.
namespace e33::platform
{
// Chamado uma vez, na primeira vez que houver um quadro para desenhar, já com
// o contexto do ImGui criado. É onde estilo e fonte são carregados: o atlas
// existe e ainda não foi usado.
using InitFn = std::function<void()>;

// Chamado a cada quadro, entre NewFrame e Render.
using RenderFn = std::function<void()>;

struct Config
{
    std::filesystem::path mod_dir{};
    InitFn on_first_frame{};
    RenderFn on_render{};
};

// Instala os hooks. Devolve false se não conseguiu descobrir a swapchain — e
// aí o log diz qual renderizador foi encontrado.
bool install(Config config);
void uninstall();

// true enquanto o overlay captura mouse e teclado. Enquanto isso o input não
// deve chegar ao jogo, senão clicar num botão também gira a câmera.
[[nodiscard]] bool wants_input();
void set_wants_input(bool wants);

// Borda de descida de uma tecla. Segurar não pode ficar alternando a janela a
// cada quadro.
[[nodiscard]] bool key_pressed(int virtual_key);
} // namespace e33::platform
