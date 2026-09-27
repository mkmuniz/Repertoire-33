#include "UI/Theme.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "Support/Log.hpp"

namespace e33::ui::theme
{
namespace
{
ImFont* g_display{nullptr};
ImFont* g_body{nullptr};
ImFont* g_small{nullptr};

constexpr float kLetterSpacing = 2.5f;   // caixa alta sem espaçamento fica apertada
constexpr float kOrnamentLength = 14.0f; // cantoneira
constexpr float kFleuronRadius = 3.5f;

ImU32 u32(const ImVec4& color)
{
    return ImGui::ColorConvertFloat4ToU32(color);
}
} // namespace

void apply_style()
{
    auto& style = ImGui::GetStyle();

    // Quinas vivas em tudo: moldura da Belle Époque é esquadriada, e o raio
    // arredondado padrão do ImGui é o que mais denuncia "isto é um overlay".
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    // Respiro generoso: a UI do jogo é espaçada, não densa.
    style.WindowPadding = ImVec2{16.0f, 14.0f};
    style.FramePadding = ImVec2{10.0f, 6.0f};
    style.CellPadding = ImVec2{10.0f, 6.0f};
    style.ItemSpacing = ImVec2{10.0f, 8.0f};
    style.ItemInnerSpacing = ImVec2{8.0f, 6.0f};
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 10.0f;

    style.WindowTitleAlign = ImVec2{0.5f, 0.5f}; // título centrado, como placa

    auto* c = style.Colors;
    c[ImGuiCol_Text] = color::kBone;
    c[ImGuiCol_TextDisabled] = color::kAsh;
    c[ImGuiCol_WindowBg] = color::kVoid;
    c[ImGuiCol_ChildBg] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_PopupBg] = color::kObsidian;

    c[ImGuiCol_Border] = color::kGoldDim;
    c[ImGuiCol_BorderShadow] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};

    c[ImGuiCol_FrameBg] = color::kMarble;
    c[ImGuiCol_FrameBgHovered] = color::kMarbleLit;
    c[ImGuiCol_FrameBgActive] = color::kMarbleLit;

    c[ImGuiCol_TitleBg] = color::kObsidian;
    c[ImGuiCol_TitleBgActive] = color::kObsidian;
    c[ImGuiCol_TitleBgCollapsed] = color::kVoid;
    c[ImGuiCol_MenuBarBg] = color::kObsidian;

    c[ImGuiCol_ScrollbarBg] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_ScrollbarGrab] = color::kGoldDim;
    c[ImGuiCol_ScrollbarGrabHovered] = color::kGold;
    c[ImGuiCol_ScrollbarGrabActive] = color::kGoldBright;

    c[ImGuiCol_CheckMark] = color::kGold;
    c[ImGuiCol_SliderGrab] = color::kGold;
    c[ImGuiCol_SliderGrabActive] = color::kGoldBright;

    // Botão sem preenchimento: só a borda. O ouro entra no hover.
    c[ImGuiCol_Button] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_ButtonHovered] = color::kGoldGhost;
    c[ImGuiCol_ButtonActive] = ImVec4{color::kGold.x, color::kGold.y, color::kGold.z, 0.32f};

    c[ImGuiCol_Header] = color::kGoldGhost;
    c[ImGuiCol_HeaderHovered] = ImVec4{color::kGold.x, color::kGold.y, color::kGold.z, 0.24f};
    c[ImGuiCol_HeaderActive] = ImVec4{color::kGold.x, color::kGold.y, color::kGold.z, 0.32f};

    c[ImGuiCol_Separator] = color::kGoldDim;
    c[ImGuiCol_SeparatorHovered] = color::kGold;
    c[ImGuiCol_SeparatorActive] = color::kGoldBright;

    c[ImGuiCol_ResizeGrip] = color::kGoldDim;
    c[ImGuiCol_ResizeGripHovered] = color::kGold;
    c[ImGuiCol_ResizeGripActive] = color::kGoldBright;

    // Aba ativa marcada por sublinhado em ouro (desenhado à mão), não por
    // retângulo preenchido: o ImGui preenchido parece software, não moldura.
    c[ImGuiCol_Tab] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_TabHovered] = color::kGoldGhost;
    c[ImGuiCol_TabSelected] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_TabDimmed] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_TabDimmedSelected] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};

    c[ImGuiCol_TableHeaderBg] = color::kObsidian;
    c[ImGuiCol_TableBorderStrong] = color::kGoldDim;
    c[ImGuiCol_TableBorderLight] = ImVec4{color::kGoldDim.x, color::kGoldDim.y,
                                          color::kGoldDim.z, 0.45f};
    c[ImGuiCol_TableRowBg] = ImVec4{0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_TableRowBgAlt] = ImVec4{1.0f, 1.0f, 1.0f, 0.02f};

    c[ImGuiCol_PlotHistogram] = color::kGold;
    c[ImGuiCol_PlotHistogramHovered] = color::kGoldBright;
    c[ImGuiCol_TextSelectedBg] = color::kGoldGhost;
    c[ImGuiCol_NavCursor] = color::kGold;
}

bool load_fonts(const std::filesystem::path& assets_dir, float base_size)
{
    const auto regular = assets_dir / "fonts" / "EBGaramond.ttf";
    std::error_code ec;
    if (!std::filesystem::exists(regular, ec))
    {
        log::warn("fonte nao encontrada em {}; usando a fonte padrao do ImGui",
                  regular.string());
        return false;
    }

    auto& io = ImGui::GetIO();

    // Latin-1 e Latin Extended-A: os nomes do jogo sao franceses, e sem este
    // range "Sirène" sai como "Sirne". Foi o risco anotado quando o overlay
    // nasceu; a fonte propria e o que o resolve.
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Latin basico + suplemento
        0x0100, 0x017F, // Latin Extended-A
        0x2010, 0x2027, // travessoes e aspas tipograficas
        0,
    };

    ImFontConfig config;
    config.OversampleH = 3; // serifa fina precisa de oversampling para nao sumir
    config.OversampleV = 2;
    config.PixelSnapH = false;

    const auto path = regular.string();
    g_body = io.Fonts->AddFontFromFileTTF(path.c_str(), base_size, &config, ranges);
    g_display = io.Fonts->AddFontFromFileTTF(path.c_str(), base_size * 1.35f, &config, ranges);
    g_small = io.Fonts->AddFontFromFileTTF(path.c_str(), base_size * 0.86f, &config, ranges);

    if (g_body == nullptr)
    {
        log::warn("falha ao carregar {}", path);
        return false;
    }
    io.FontDefault = g_body;
    log::info("fonte carregada: {}", path);
    return true;
}

bool fonts_loaded()
{
    return g_body != nullptr;
}

void push_display_font()
{
    if (g_display != nullptr)
    {
        ImGui::PushFont(g_display);
    }
}

void push_small_font()
{
    if (g_small != nullptr)
    {
        ImGui::PushFont(g_small);
    }
}

void pop_font()
{
    if (g_body != nullptr)
    {
        ImGui::PopFont();
    }
}

void heading(std::string_view text)
{
    auto* draw = ImGui::GetWindowDrawList();
    const auto origin = ImGui::GetCursorScreenPos();
    const auto font_size = ImGui::GetFontSize();

    // Caixa alta com espacamento manual: o ImGui nao tem letter-spacing, e e
    // justamente o espacamento que da o ar de titulo gravado.
    float x = origin.x;
    for (const char raw : text)
    {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        const char glyph[2] = {upper, '\0'};
        draw->AddText(ImVec2{x, origin.y}, u32(color::kGold), glyph, glyph + 1);
        x += ImGui::CalcTextSize(glyph).x + kLetterSpacing;
    }

    ImGui::Dummy(ImVec2{x - origin.x, font_size});
}

void rule(float fraction)
{
    auto* draw = ImGui::GetWindowDrawList();
    const auto available = ImGui::GetContentRegionAvail().x;
    const auto width = available * std::clamp(fraction, 0.05f, 1.0f);
    const auto origin = ImGui::GetCursorScreenPos();
    const auto y = origin.y + 6.0f;
    const auto left = origin.x;
    const auto right = origin.x + width;
    const auto middle = (left + right) * 0.5f;

    const auto gold = u32(color::kGoldDim);
    const auto bright = u32(color::kGold);

    // Duas linhas com um losango ao centro: o fio simples parece divisor de
    // formulario; o losango e o que remete a ornamento de moldura.
    draw->AddLine(ImVec2{left, y}, ImVec2{middle - kFleuronRadius * 2.4f, y}, gold, 1.0f);
    draw->AddLine(ImVec2{middle + kFleuronRadius * 2.4f, y}, ImVec2{right, y}, gold, 1.0f);

    const ImVec2 diamond[4] = {
        ImVec2{middle, y - kFleuronRadius},
        ImVec2{middle + kFleuronRadius, y},
        ImVec2{middle, y + kFleuronRadius},
        ImVec2{middle - kFleuronRadius, y},
    };
    draw->AddConvexPolyFilled(diamond, 4, bright);

    ImGui::Dummy(ImVec2{width, 12.0f});
}

void window_ornaments()
{
    auto* draw = ImGui::GetWindowDrawList();
    const auto min = ImGui::GetWindowPos();
    const auto size = ImGui::GetWindowSize();
    const ImVec2 max{min.x + size.x, min.y + size.y};
    const auto gold = u32(color::kGold);
    const auto inset = 3.0f;
    const auto len = kOrnamentLength;

    // Cantoneiras: duas hastes por canto, por dentro da borda da janela.
    const ImVec2 tl{min.x + inset, min.y + inset};
    const ImVec2 tr{max.x - inset, min.y + inset};
    const ImVec2 bl{min.x + inset, max.y - inset};
    const ImVec2 br{max.x - inset, max.y - inset};

    draw->AddLine(tl, ImVec2{tl.x + len, tl.y}, gold, 1.0f);
    draw->AddLine(tl, ImVec2{tl.x, tl.y + len}, gold, 1.0f);

    draw->AddLine(tr, ImVec2{tr.x - len, tr.y}, gold, 1.0f);
    draw->AddLine(tr, ImVec2{tr.x, tr.y + len}, gold, 1.0f);

    draw->AddLine(bl, ImVec2{bl.x + len, bl.y}, gold, 1.0f);
    draw->AddLine(bl, ImVec2{bl.x, bl.y - len}, gold, 1.0f);

    draw->AddLine(br, ImVec2{br.x - len, br.y}, gold, 1.0f);
    draw->AddLine(br, ImVec2{br.x, br.y - len}, gold, 1.0f);
}

void text_dim(std::string_view text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color::kBoneDim);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopStyleColor();
}

void text_gold(std::string_view text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color::kGold);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::PopStyleColor();
}

void text_value(double value, const char* fmt)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color::kGoldBright);
    ImGui::Text(fmt, value);
    ImGui::PopStyleColor();
}

bool button(const char* label, const ImVec2& size)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color::kGold);
    ImGui::PushStyleColor(ImGuiCol_Border, color::kGoldDim);
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(2);
    return pressed;
}

bool checkbox(const char* label, bool* value)
{
    const auto box = ImGui::GetFrameHeight() * 0.78f;
    const auto origin = ImGui::GetCursorScreenPos();
    const auto id = std::string{"##"} + label;

    const bool pressed = ImGui::InvisibleButton(id.c_str(), ImVec2{box, box});
    if (pressed)
    {
        *value = !*value;
    }
    const bool hovered = ImGui::IsItemHovered();

    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = origin;
    const ImVec2 max{origin.x + box, origin.y + box};
    draw->AddRectFilled(min, max, u32(hovered ? color::kMarbleLit : color::kMarble));
    draw->AddRect(min, max, u32(hovered ? color::kGold : color::kGoldDim), 0.0f, 0, 1.0f);

    if (*value)
    {
        const auto cx = (min.x + max.x) * 0.5f;
        const auto cy = (min.y + max.y) * 0.5f;
        const auto r = box * 0.28f;
        const ImVec2 diamond[4] = {
            ImVec2{cx, cy - r}, ImVec2{cx + r, cy}, ImVec2{cx, cy + r}, ImVec2{cx - r, cy},
        };
        draw->AddConvexPolyFilled(diamond, 4, u32(color::kGoldBright));
    }

    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    return pressed;
}

bool row(const char* id, bool selected, float height)
{
    const auto line = height > 0.0f ? height : ImGui::GetTextLineHeightWithSpacing();
    const auto origin = ImGui::GetCursorScreenPos();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{1.0f, 1.0f, 1.0f, 0.04f});
    const bool clicked = ImGui::Selectable(id, selected,
                                           ImGuiSelectableFlags_AllowOverlap,
                                           ImVec2{0.0f, line});
    ImGui::PopStyleColor();

    if (selected)
    {
        // Barra dourada à esquerda em vez do preenchimento azul: marca a
        // seleção sem cobrir o texto de cor.
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(ImVec2{origin.x - 4.0f, origin.y},
                            ImVec2{origin.x - 1.0f, origin.y + line},
                            u32(color::kGoldBright));
    }
    return clicked;
}
} // namespace e33::ui::theme
