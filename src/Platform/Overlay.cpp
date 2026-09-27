#include "Platform/Overlay.hpp"

#if defined(_WIN32)

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <MinHook.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include "Support/Log.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam,
                                                             LPARAM lparam);

namespace e33::platform
{
namespace
{
// Índices nas vtables da DXGI/D3D12. São estáveis porque COM proíbe reordenar
// métodos de uma interface publicada — é essa garantia que torna o hook por
// vtable viável sem varrer assinatura de bytes.
constexpr std::size_t kPresentIndex = 8;          // IDXGISwapChain::Present
constexpr std::size_t kResizeBuffersIndex = 13;   // IDXGISwapChain::ResizeBuffers
constexpr std::size_t kExecuteCommandListsIndex = 10; // ID3D12CommandQueue

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT,
                                                    DXGI_FORMAT, UINT);
using ExecuteCommandListsFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT,
                                                       ID3D12CommandList* const*);

Config g_config{};
std::atomic<bool> g_wants_input{false};
std::atomic<bool> g_initialized{false};
std::mutex g_init_mutex;

PresentFn g_present_original{nullptr};
ResizeBuffersFn g_resize_original{nullptr};
ExecuteCommandListsFn g_execute_original{nullptr};

HWND g_window{nullptr};
WNDPROC g_original_wndproc{nullptr};

// D3D11
ID3D11Device* g_d3d11_device{nullptr};
ID3D11DeviceContext* g_d3d11_context{nullptr};
ID3D11RenderTargetView* g_d3d11_rtv{nullptr};

// D3D12
ID3D12Device* g_d3d12_device{nullptr};
ID3D12CommandQueue* g_d3d12_queue{nullptr};
ID3D12DescriptorHeap* g_d3d12_srv_heap{nullptr};
ID3D12DescriptorHeap* g_d3d12_rtv_heap{nullptr};
ID3D12GraphicsCommandList* g_d3d12_command_list{nullptr};
struct FrameContext
{
    ID3D12CommandAllocator* allocator{nullptr};
    ID3D12Resource* render_target{nullptr};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle{};
};
std::vector<FrameContext> g_frames;

bool g_is_d3d12{false};

std::array<bool, 256> g_key_was_down{};

LRESULT CALLBACK hooked_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (g_initialized.load(std::memory_order_acquire))
    {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

        // Enquanto o overlay está aberto o input é dele. Deixar passar faria
        // um clique num botão girar a câmera junto.
        if (g_wants_input.load(std::memory_order_relaxed))
        {
            const auto& io = ImGui::GetIO();
            const bool mouse = io.WantCaptureMouse
                               && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST);
            const bool keyboard = io.WantCaptureKeyboard
                                  && (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR
                                      || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP);
            if (mouse || keyboard)
            {
                return TRUE;
            }
        }
    }
    return CallWindowProcW(g_original_wndproc, hwnd, msg, wparam, lparam);
}

void release_render_targets()
{
    if (g_d3d11_rtv != nullptr)
    {
        g_d3d11_rtv->Release();
        g_d3d11_rtv = nullptr;
    }
    for (auto& frame : g_frames)
    {
        if (frame.render_target != nullptr)
        {
            frame.render_target->Release();
            frame.render_target = nullptr;
        }
    }
}

bool setup_d3d11(IDXGISwapChain* swapchain)
{
    if (FAILED(swapchain->GetDevice(IID_PPV_ARGS(&g_d3d11_device))))
    {
        return false;
    }
    g_d3d11_device->GetImmediateContext(&g_d3d11_context);
    ImGui_ImplDX11_Init(g_d3d11_device, g_d3d11_context);
    return true;
}

bool setup_d3d12(IDXGISwapChain3* swapchain)
{
    if (FAILED(swapchain->GetDevice(IID_PPV_ARGS(&g_d3d12_device))))
    {
        return false;
    }
    if (g_d3d12_queue == nullptr)
    {
        // Sem a command queue o backend DX12 não tem como submeter. Ela só
        // aparece pelo hook de ExecuteCommandLists, então o overlay começa a
        // desenhar um ou dois quadros depois do primeiro Present.
        log::warn("command queue do D3D12 ainda nao capturada; aguardando");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    swapchain->GetDesc(&desc);
    const auto buffer_count = desc.BufferCount;

    D3D12_DESCRIPTOR_HEAP_DESC srv_desc{};
    srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_desc.NumDescriptors = buffer_count;
    srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(g_d3d12_device->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&g_d3d12_srv_heap))))
    {
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
    rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.NumDescriptors = buffer_count;
    rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(g_d3d12_device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&g_d3d12_rtv_heap))))
    {
        return false;
    }

    const auto rtv_size =
        g_d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto rtv_handle = g_d3d12_rtv_heap->GetCPUDescriptorHandleForHeapStart();

    g_frames.resize(buffer_count);
    for (UINT i = 0; i < buffer_count; ++i)
    {
        g_frames[i].rtv_handle = rtv_handle;
        rtv_handle.ptr += rtv_size;

        if (FAILED(g_d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                          IID_PPV_ARGS(&g_frames[i].allocator))))
        {
            return false;
        }
        if (SUCCEEDED(swapchain->GetBuffer(i, IID_PPV_ARGS(&g_frames[i].render_target))))
        {
            g_d3d12_device->CreateRenderTargetView(g_frames[i].render_target, nullptr,
                                                   g_frames[i].rtv_handle);
        }
    }

    if (FAILED(g_d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                 g_frames[0].allocator, nullptr,
                                                 IID_PPV_ARGS(&g_d3d12_command_list))))
    {
        return false;
    }
    g_d3d12_command_list->Close();

    ImGui_ImplDX12_Init(g_d3d12_device, static_cast<int>(buffer_count),
                        DXGI_FORMAT_R8G8B8A8_UNORM, g_d3d12_srv_heap,
                        g_d3d12_srv_heap->GetCPUDescriptorHandleForHeapStart(),
                        g_d3d12_srv_heap->GetGPUDescriptorHandleForHeapStart());
    return true;
}

bool initialize(IDXGISwapChain* swapchain)
{
    const std::lock_guard lock{g_init_mutex};
    if (g_initialized.load(std::memory_order_acquire))
    {
        return true;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapchain->GetDesc(&desc)))
    {
        return false;
    }
    g_window = desc.OutputWindow;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // posição vai para o nosso próprio arquivo
    ImGui_ImplWin32_Init(g_window);

    // O jogo é UE5, então D3D12 é o caminho provável; D3D11 existe porque o
    // usuário pode forçar -dx11 na linha de comando.
    IDXGISwapChain3* swapchain3{nullptr};
    if (SUCCEEDED(swapchain->QueryInterface(IID_PPV_ARGS(&swapchain3))))
    {
        ID3D12Device* device12{nullptr};
        if (SUCCEEDED(swapchain3->GetDevice(IID_PPV_ARGS(&device12))))
        {
            device12->Release();
            g_is_d3d12 = true;
            if (!setup_d3d12(swapchain3))
            {
                swapchain3->Release();
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                return false;
            }
        }
        swapchain3->Release();
    }

    if (!g_is_d3d12 && !setup_d3d11(swapchain))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        log::warn("nem D3D12 nem D3D11 reconhecidos nesta swapchain");
        return false;
    }

    g_original_wndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_window, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(hooked_wndproc)));

    if (g_config.on_first_frame)
    {
        g_config.on_first_frame();
    }

    g_initialized.store(true, std::memory_order_release);
    log::info("overlay inicializado ({})", g_is_d3d12 ? "D3D12" : "D3D11");
    return true;
}

void render_d3d11(IDXGISwapChain* swapchain)
{
    if (g_d3d11_rtv == nullptr)
    {
        ID3D11Texture2D* back_buffer{nullptr};
        if (FAILED(swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
        {
            return;
        }
        g_d3d11_device->CreateRenderTargetView(back_buffer, nullptr, &g_d3d11_rtv);
        back_buffer->Release();
    }
    g_d3d11_context->OMSetRenderTargets(1, &g_d3d11_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void render_d3d12(IDXGISwapChain* swapchain)
{
    IDXGISwapChain3* swapchain3{nullptr};
    if (FAILED(swapchain->QueryInterface(IID_PPV_ARGS(&swapchain3))))
    {
        return;
    }
    const auto index = swapchain3->GetCurrentBackBufferIndex();
    swapchain3->Release();

    if (index >= g_frames.size() || g_frames[index].render_target == nullptr)
    {
        return;
    }
    auto& frame = g_frames[index];

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = frame.render_target;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    frame.allocator->Reset();
    g_d3d12_command_list->Reset(frame.allocator, nullptr);
    g_d3d12_command_list->ResourceBarrier(1, &barrier);
    g_d3d12_command_list->OMSetRenderTargets(1, &frame.rtv_handle, FALSE, nullptr);
    g_d3d12_command_list->SetDescriptorHeaps(1, &g_d3d12_srv_heap);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_d3d12_command_list);

    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    g_d3d12_command_list->ResourceBarrier(1, &barrier);
    g_d3d12_command_list->Close();

    ID3D12CommandList* lists[] = {g_d3d12_command_list};
    g_d3d12_queue->ExecuteCommandLists(1, lists);
}

HRESULT STDMETHODCALLTYPE hooked_present(IDXGISwapChain* swapchain, UINT interval, UINT flags)
{
    if (!g_initialized.load(std::memory_order_acquire) && !initialize(swapchain))
    {
        return g_present_original(swapchain, interval, flags);
    }

    if (g_is_d3d12)
    {
        ImGui_ImplDX12_NewFrame();
    }
    else
    {
        ImGui_ImplDX11_NewFrame();
    }
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_config.on_render)
    {
        g_config.on_render();
    }

    ImGui::Render();
    if (g_is_d3d12)
    {
        render_d3d12(swapchain);
    }
    else
    {
        render_d3d11(swapchain);
    }

    return g_present_original(swapchain, interval, flags);
}

HRESULT STDMETHODCALLTYPE hooked_resize_buffers(IDXGISwapChain* swapchain, UINT count,
                                                UINT width, UINT height, DXGI_FORMAT format,
                                                UINT flags)
{
    // Alt-tab, mudança de resolução e troca de modo de janela passam por aqui.
    // Segurar render target de um buffer que deixou de existir trava o jogo.
    release_render_targets();
    const auto result = g_resize_original(swapchain, count, width, height, format, flags);

    if (g_initialized.load(std::memory_order_acquire) && g_is_d3d12)
    {
        IDXGISwapChain3* swapchain3{nullptr};
        if (SUCCEEDED(swapchain->QueryInterface(IID_PPV_ARGS(&swapchain3))))
        {
            for (std::size_t i = 0; i < g_frames.size(); ++i)
            {
                if (SUCCEEDED(swapchain3->GetBuffer(static_cast<UINT>(i),
                                                    IID_PPV_ARGS(&g_frames[i].render_target))))
                {
                    g_d3d12_device->CreateRenderTargetView(g_frames[i].render_target, nullptr,
                                                           g_frames[i].rtv_handle);
                }
            }
            swapchain3->Release();
        }
    }
    return result;
}

void STDMETHODCALLTYPE hooked_execute_command_lists(ID3D12CommandQueue* queue, UINT count,
                                                    ID3D12CommandList* const* lists)
{
    // Única forma de obter a command queue que o jogo usa. O backend DX12 do
    // ImGui precisa dela, e não há API para perguntar à swapchain qual é.
    if (g_d3d12_queue == nullptr && queue != nullptr)
    {
        D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
        if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
        {
            g_d3d12_queue = queue;
        }
    }
    g_execute_original(queue, count, lists);
}

// Cria dispositivo e swapchain descartáveis só para ler os ponteiros das
// vtables. É mais estável que procurar assinatura de bytes, que muda a cada
// driver.
bool capture_vtables(void** present_out, void** resize_out, void** execute_out)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"E33OverlayProbe";
    if (RegisterClassExW(&wc) == 0)
    {
        return false;
    }
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16,
                                  nullptr, nullptr, wc.hInstance, nullptr);
    if (window == nullptr)
    {
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    bool ok = false;

    // --- D3D12 ---
    ID3D12Device* device12{nullptr};
    if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12))))
    {
        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ID3D12CommandQueue* queue{nullptr};
        if (SUCCEEDED(device12->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue))))
        {
            *execute_out = (*reinterpret_cast<void***>(queue))[kExecuteCommandListsIndex];

            IDXGIFactory4* factory{nullptr};
            if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            {
                DXGI_SWAP_CHAIN_DESC1 sc_desc{};
                sc_desc.BufferCount = 2;
                sc_desc.Width = 16;
                sc_desc.Height = 16;
                sc_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                sc_desc.SampleDesc.Count = 1;

                IDXGISwapChain1* swapchain{nullptr};
                if (SUCCEEDED(factory->CreateSwapChainForHwnd(queue, window, &sc_desc, nullptr,
                                                              nullptr, &swapchain)))
                {
                    auto** vtable = *reinterpret_cast<void***>(swapchain);
                    *present_out = vtable[kPresentIndex];
                    *resize_out = vtable[kResizeBuffersIndex];
                    swapchain->Release();
                    ok = true;
                }
                factory->Release();
            }
            queue->Release();
        }
        device12->Release();
    }

    // --- D3D11, se o D3D12 não estiver disponível ---
    if (!ok)
    {
        DXGI_SWAP_CHAIN_DESC sc_desc{};
        sc_desc.BufferCount = 1;
        sc_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sc_desc.OutputWindow = window;
        sc_desc.SampleDesc.Count = 1;
        sc_desc.Windowed = TRUE;
        sc_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* swapchain{nullptr};
        ID3D11Device* device{nullptr};
        ID3D11DeviceContext* context{nullptr};
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                                    0, &level, 1, D3D11_SDK_VERSION, &sc_desc,
                                                    &swapchain, &device, nullptr, &context)))
        {
            auto** vtable = *reinterpret_cast<void***>(swapchain);
            *present_out = vtable[kPresentIndex];
            *resize_out = vtable[kResizeBuffersIndex];
            swapchain->Release();
            device->Release();
            if (context != nullptr)
            {
                context->Release();
            }
            ok = true;
        }
    }

    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return ok;
}
} // namespace

bool install(Config config)
{
    g_config = std::move(config);

    if (MH_Initialize() != MH_OK)
    {
        log::warn("MinHook nao inicializou");
        return false;
    }

    void* present{nullptr};
    void* resize{nullptr};
    void* execute{nullptr};
    if (!capture_vtables(&present, &resize, &execute))
    {
        log::warn("nao foi possivel descobrir a swapchain do jogo");
        return false;
    }

    if (MH_CreateHook(present, reinterpret_cast<void*>(&hooked_present),
                      reinterpret_cast<void**>(&g_present_original))
        != MH_OK)
    {
        return false;
    }
    if (resize != nullptr)
    {
        MH_CreateHook(resize, reinterpret_cast<void*>(&hooked_resize_buffers),
                      reinterpret_cast<void**>(&g_resize_original));
    }
    if (execute != nullptr)
    {
        MH_CreateHook(execute, reinterpret_cast<void*>(&hooked_execute_command_lists),
                      reinterpret_cast<void**>(&g_execute_original));
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
    {
        log::warn("falha ao ativar os hooks");
        return false;
    }
    log::info("hooks instalados");
    return true;
}

void uninstall()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_window != nullptr && g_original_wndproc != nullptr)
    {
        SetWindowLongPtrW(g_window, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_original_wndproc));
    }
    if (g_initialized.exchange(false, std::memory_order_acq_rel))
    {
        if (g_is_d3d12)
        {
            ImGui_ImplDX12_Shutdown();
        }
        else
        {
            ImGui_ImplDX11_Shutdown();
        }
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    release_render_targets();
}

bool wants_input()
{
    return g_wants_input.load(std::memory_order_relaxed);
}

void set_wants_input(bool wants)
{
    g_wants_input.store(wants, std::memory_order_relaxed);
}

bool key_pressed(int virtual_key)
{
    if (virtual_key <= 0 || virtual_key >= static_cast<int>(g_key_was_down.size()))
    {
        return false;
    }
    const bool down = (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
    const bool pressed = down && !g_key_was_down[static_cast<std::size_t>(virtual_key)];
    g_key_was_down[static_cast<std::size_t>(virtual_key)] = down;
    return pressed;
}
} // namespace e33::platform

#else // !_WIN32

// Fora do Windows não há jogo para enganchar. O stub existe para que a camada
// de lógica continue compilando e sendo testada em qualquer sistema.
namespace e33::platform
{
bool install(Config) { return false; }
void uninstall() {}
bool wants_input() { return false; }
void set_wants_input(bool) {}
bool key_pressed(int) { return false; }
} // namespace e33::platform

#endif
