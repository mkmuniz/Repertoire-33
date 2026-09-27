#pragma once

#include <filesystem>
#include <string_view>

#include <imgui.h>

// Estilo visual dos overlays, fiel à direção de arte de Clair Obscur.
//
// O que o jogo faz, e o que este arquivo reproduz:
//
//   Obsidiana e mármore negro     fundos quase pretos, com um leve calor —
//                                 preto puro é frio, o do jogo puxa sépia.
//   Ouro metálico                 único acento. Nunca preenche área grande;
//                                 aparece em fio, borda e texto de destaque,
//                                 que é como ouro se comporta numa moldura.
//   Clair-obscur (claro-escuro)   contraste alto e deliberado: texto cor de
//                                 osso sobre preto profundo, sem meios-tons
//                                 cinzentos que achatariam a cena.
//   Art Nouveau                   serifa, caixa alta espaçada, quinas vivas,
//                                 ornamento discreto. Nada arredondado: as
//                                 molduras da Belle Époque são esquadriadas.
//
// O que NÃO dá para reproduzir e por quê: a fonte do jogo é de terceiros e não
// pode ser redistribuída, então vai EB Garamond (OFL) no lugar; e ornamento
// floral de verdade exigiria textura, o que numa DLL injetada custa mais do que
// entrega — o desenho aqui é todo vetorial, feito com o draw list.
namespace e33::ui::theme
{
namespace color
{
// Retirados da paleta de materiais do jogo: obsidiana, mármore negro, ouro.
constexpr ImVec4 kVoid{0.043f, 0.039f, 0.035f, 1.00f};        // fundo da janela
constexpr ImVec4 kObsidian{0.075f, 0.067f, 0.059f, 1.00f};    // painel
constexpr ImVec4 kMarble{0.110f, 0.098f, 0.086f, 1.00f};      // campo, linha alternada
constexpr ImVec4 kMarbleLit{0.145f, 0.129f, 0.110f, 1.00f};   // hover

constexpr ImVec4 kGold{0.816f, 0.667f, 0.318f, 1.00f};        // acento principal
constexpr ImVec4 kGoldBright{0.925f, 0.804f, 0.494f, 1.00f};  // brilho, seleção
constexpr ImVec4 kGoldDim{0.478f, 0.388f, 0.196f, 1.00f};     // fio, borda
constexpr ImVec4 kGoldGhost{0.816f, 0.667f, 0.318f, 0.16f};   // preenchimento sutil

constexpr ImVec4 kBone{0.902f, 0.878f, 0.824f, 1.00f};        // texto
constexpr ImVec4 kBoneDim{0.612f, 0.584f, 0.529f, 1.00f};     // texto secundário
constexpr ImVec4 kAsh{0.376f, 0.357f, 0.325f, 1.00f};         // desabilitado

constexpr ImVec4 kBlood{0.643f, 0.243f, 0.220f, 1.00f};       // erro, perda
constexpr ImVec4 kVerdigris{0.478f, 0.627f, 0.553f, 1.00f};   // ganho
} // namespace color

// Aplica a paleta e a geometria ao ImGuiStyle atual.
void apply_style();

// Carrega EB Garamond de `assets/fonts/`. Devolve false se não achou — e aí o
// overlay usa a fonte padrão do ImGui, feio mas funcional.
//
// O glyph range inclui Latin-1 e Latin Extended-A de propósito: os nomes do
// jogo são franceses, e a fonte padrão do ImGui não tem "è" nem "É".
bool load_fonts(const std::filesystem::path& assets_dir, float base_size = 17.0f);
[[nodiscard]] bool fonts_loaded();

void push_display_font();   // títulos
void push_small_font();     // notas, rodapé
void pop_font();

// ---- elementos ----

// Título de seção: caixa alta, espaçada, em ouro. Só ASCII (são rótulos nossos).
void heading(std::string_view text);

// Fio ornamental: duas linhas finas com um losango ao centro. `fraction` é a
// largura em relação ao espaço disponível.
void rule(float fraction = 1.0f);

// Cantoneiras douradas nos quatro cantos da janela atual. Chamar logo após o
// Begin, antes do conteúdo.
void window_ornaments();

// Texto nos tons do tema.
void text_dim(std::string_view text);
void text_gold(std::string_view text);
void text_value(double value, const char* fmt);

// Botão esquadriado, contorno em ouro, preenchimento só no hover.
bool button(const char* label, const ImVec2& size = ImVec2{0.0f, 0.0f});

// Linha selecionável com barra dourada à esquerda quando ativa, em vez do
// preenchimento azul do ImGui.
bool row(const char* id, bool selected, float height = 0.0f);

// Caixa de marcação com losango no lugar do "tique". O tique do ImGui é o
// elemento mais barulhento da tela quando pintado de ouro, e o losango repete o
// motivo que já está nos fios ornamentais.
bool checkbox(const char* label, bool* value);
} // namespace e33::ui::theme
