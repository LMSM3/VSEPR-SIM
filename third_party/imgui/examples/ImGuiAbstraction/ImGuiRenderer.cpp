#include "ImGuiRenderer.h"
#include "imgui.h"

#ifdef _WIN32
    #include "backends/imgui_impl_win32.h"
    #include "backends/imgui_impl_dx11.h"
    #include <d3d11.h>
    #include <tchar.h>
    #pragma comment(lib, "d3d11.lib")
    
    #ifndef WM_DPICHANGED
        #define WM_DPICHANGED 0x02E0
    #endif
    
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace ImGuiAbstraction {

#ifdef _WIN32
// DirectX11 + Win32 Implementation
class DX11Win32Renderer : public IRenderer {
private:
    HWND hwnd = nullptr;
    WNDCLASSEXW wc = {};
    ID3D11Device* pd3dDevice = nullptr;
    ID3D11DeviceContext* pd3dDeviceContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11RenderTargetView* mainRenderTargetView = nullptr;
    bool shouldClose = false;
    RendererConfig config;
    
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
            
        switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED)
                return 0;
            // Handle resize
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        case WM_DPICHANGED:
            #ifdef ImGuiConfigFlags_DpiEnableScaleViewports
            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports) {
                const RECT* suggested_rect = (RECT*)lParam;
                ::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, 
                             suggested_rect->right - suggested_rect->left, 
                             suggested_rect->bottom - suggested_rect->top, 
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            #endif
            break;
        }
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    
    bool CreateDeviceD3D() {
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        
        UINT createDeviceFlags = 0;
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        
        HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, 
                                                     featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &pSwapChain, 
                                                     &pd3dDevice, &featureLevel, &pd3dDeviceContext);
        if (res != S_OK)
            return false;
            
        CreateRenderTarget();
        return true;
    }
    
    void CreateRenderTarget() {
        ID3D11Texture2D* pBackBuffer;
        pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (pBackBuffer) {
            pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
            pBackBuffer->Release();
        }
    }
    
    void CleanupRenderTarget() {
        if (mainRenderTargetView) { 
            mainRenderTargetView->Release(); 
            mainRenderTargetView = nullptr; 
        }
    }
    
    void CleanupDeviceD3D() {
        CleanupRenderTarget();
        if (pSwapChain) { pSwapChain->Release(); pSwapChain = nullptr; }
        if (pd3dDeviceContext) { pd3dDeviceContext->Release(); pd3dDeviceContext = nullptr; }
        if (pd3dDevice) { pd3dDevice->Release(); pd3dDevice = nullptr; }
    }
    
public:
    bool Init(const RendererConfig& cfg) override {
        config = cfg;
        
        // Create window class
        wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGuiAbstractionClass", nullptr };
        ::RegisterClassExW(&wc);
        
        // Create window
        hwnd = ::CreateWindowW(wc.lpszClassName, 
                              std::wstring(config.windowTitle.begin(), config.windowTitle.end()).c_str(), 
                              WS_OVERLAPPEDWINDOW, 100, 100, config.windowWidth, config.windowHeight, 
                              nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd)
            return false;
            
        // Initialize Direct3D
        if (!CreateDeviceD3D()) {
            CleanupDeviceD3D();
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }
        
        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);
        
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        
        ImGui::StyleColorsDark();
        
        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(pd3dDevice, pd3dDeviceContext);
        
        return true;
    }
    
    void Shutdown() override {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    }
    
    void NewFrame() override {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }
    
    void Render() override {
        ImGui::Render();
        const float clear_color[4] = { 0.45f, 0.55f, 0.60f, 1.00f };
        pd3dDeviceContext->OMSetRenderTargets(1, &mainRenderTargetView, nullptr);
        pd3dDeviceContext->ClearRenderTargetView(mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        
        UINT syncInterval = config.vsync ? 1 : 0;
        pSwapChain->Present(syncInterval, 0);
    }
    
    bool ShouldClose() const override {
        return shouldClose;
    }
    
    void PollEvents() override {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                shouldClose = true;
        }
    }
    
    void* GetNativeWindow() const override {
        return hwnd;
    }
    
    RendererType GetRendererType() const override {
        return RendererType::DirectX11;
    }
    
    PlatformType GetPlatformType() const override {
        return PlatformType::Win32;
    }
};
#endif

// Factory implementation
std::unique_ptr<IRenderer> CreateRenderer(const RendererConfig& config) {
    RendererConfig cfg = config;
    
#ifdef _WIN32
    // On Windows, prefer DirectX11 + Win32
    if (cfg.rendererType == RendererType::Auto)
        cfg.rendererType = RendererType::DirectX11;
    if (cfg.platformType == PlatformType::Auto)
        cfg.platformType = PlatformType::Win32;
        
    if (cfg.rendererType == RendererType::DirectX11 && cfg.platformType == PlatformType::Win32) {
        auto renderer = std::make_unique<DX11Win32Renderer>();
        if (renderer->Init(cfg))
            return renderer;
    }
#endif
    
    return nullptr;
}

bool IsRendererAvailable(RendererType type) {
#ifdef _WIN32
    if (type == RendererType::DirectX11)
        return true;
#endif
    return false;
}

bool IsPlatformAvailable(PlatformType type) {
#ifdef _WIN32
    if (type == PlatformType::Win32)
        return true;
#endif
    return false;
}

} // namespace ImGuiAbstraction
