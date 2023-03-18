#define _CRT_SECURE_NO_WARNINGS
#include "imgui.h"
#include "imgui.cpp"
#include "imgui_demo.cpp"
#include "imgui_draw.cpp"
#include "imgui_widgets.cpp"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.cpp"
#include "imgui_impl_dx11.cpp"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#pragma comment(lib, "d3d11.lib")

#include <tuple>
#include <string>
#include <optional>

namespace MyImGui {
    static IDXGISwapChain* g_pSwapChain = NULL;
    static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

    void CreateRenderTarget() {
        ID3D11Texture2D* pBackBuffer;
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }

    bool CreateDeviceD3D(HWND hWnd) {
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
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
        //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
            return false;

        CreateRenderTarget();
        return true;
    }

    void CleanupRenderTarget() {
        if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
    }

    void CleanupDeviceD3D() {
        CleanupRenderTarget();
        if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
        if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    }

    // Win32 message handler
    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg) {
        case WM_ACTIVATE:
            break;
        case WM_SIZE:
            if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }

    bool INIT(const std::u16string& windowName, WNDCLASSEXW& wc_out, HWND& hwnd_out) {
        wc_out = { sizeof(WNDCLASSEXW), CS_CLASSDC, MyImGui::WndProc, 0L, 0L, GetModuleHandleW(NULL), NULL, NULL, NULL, NULL, L"ImGui", NULL };
        ::RegisterClassExW(&wc_out);
        hwnd_out = ::CreateWindowW(wc_out.lpszClassName, (LPCWSTR)windowName.c_str(), WS_SYSMENU | WS_CAPTION | WS_MINIMIZE	| WS_SIZEBOX /*WS_OVERLAPPEDWINDOW*/, 100, 100, 800, 600, NULL, NULL, wc_out.hInstance, NULL);

        // Initialize Direct3D
        if (!MyImGui::CreateDeviceD3D(hwnd_out)) {
            MyImGui::CleanupDeviceD3D();
            ::UnregisterClassW(wc_out.lpszClassName, wc_out.hInstance);
            return false;
        }

        // Show the window
        ::ShowWindow(hwnd_out, SW_NORMAL/* SW_MAXIMIZE*/);
        ::UpdateWindow(hwnd_out);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // Setup Platform/Renderer bindings
        ImGui_ImplWin32_Init(hwnd_out);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

        float SCALE = 1.4f;
        ImFontConfig cfg;
        cfg.SizePixels = 13 * SCALE;
        ImGui::GetIO().Fonts->AddFontDefault(&cfg)->DisplayOffset.y = SCALE;

        ImGui::StyleColorsDark();

        return true;
    }

    WNDCLASSEXW wc;
    HWND hwnd;

    bool Init(const std::u16string& windowName) {
        if (!MyImGui::INIT(windowName, wc, hwnd))
            return false;
        return true;
    }

    HWND Hwnd() {
        return hwnd;
    }

    template<typename Function, typename InitFunction=void(HWND)> bool Run(Function function) {
        ImVec4 transparent_color = ImVec4(0, 0, 0, 0.5);
        MSG msg;
        ZeroMemory(&msg, sizeof(msg));
        while (msg.message != WM_QUIT) {
            if (::PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
                continue;
            }

            // Start the Dear ImGui frame
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            function();

            // Rendering
            ImGui::Render();
            g_pd3dDeviceContext->OMSetRenderTargets(1, &MyImGui::g_mainRenderTargetView, NULL);
            g_pd3dDeviceContext->ClearRenderTargetView(MyImGui::g_mainRenderTargetView, (float*)&transparent_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            MyImGui::g_pSwapChain->Present(1, 0);
        }

        // Cleanup
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        MyImGui::CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return true;
    }
}

