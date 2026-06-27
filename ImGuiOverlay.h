#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <thread>
#include <mutex>
#include <atomic>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ImGui includes (assuming ImGui is available)
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "EntityManager.h"
#include "WorldToScreen.h"
#include "Overlay.h"
#include "Color.h"

struct OverlaySettings {
    // ESP Settings
    bool espEnabled = true;
    bool espBoxes = true;
    bool espHealth = true;
    bool espName = true;
    bool espWeapon = true;
    bool espDistance = true;
    bool espSnaplines = false;
    bool espSkeleton = false;
    
    // Aimbot Settings
    bool aimbotEnabled = false;
    float aimbotSmoothing = 2.0f;
    float aimbotFOV = 15.0f;
    int aimbotBone = 0; // 0=Head, 1=Neck, 2=Chest, 3=Pelvis, 4=Feet
    float aimbotRCS = 2.0f;
    bool aimbotVisibilityCheck = false;
    bool aimbotTargetLock = false;
    int aimbotPriority = 0; // 0=FOV, 1=Distance
    
    // Misc Settings
    bool bhopEnabled = false;
    bool radarEnabled = false;
    bool noFlashEnabled = false;
    bool triggerbotEnabled = false;
    
    // Colors
    Color enemyColor = Color(255, 0, 0, 255);
    Color teammateColor = Color(0, 255, 0, 255);
    Color visibleColor = Color(255, 255, 0, 255);
};

class ImGuiOverlay {
private:
    HWND windowHandle;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;
    
    int width;
    int height;
    
    std::atomic<bool> running;
    std::atomic<bool> menuVisible;
    
    EntityManager* entityManager;
    WorldToScreen* worldToScreen;
    
    OverlaySettings settings;
    
    std::mutex dataMutex;
    std::vector<PlayerData> cachedPlayers;
    PlayerData cachedLocalPlayer;
    
    // Window procedure for ImGui
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
        
        switch (msg) {
            case WM_SIZE:
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProc(hWnd, msg, wParam, lParam);
        }
    }
    
    bool InitializeDirectX() {
        // Create swap chain
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 1;
        scd.BufferDesc.Width = width;
        scd.BufferDesc.Height = height;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate.Numerator = 144;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
        scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        scd.SampleDesc.Count = 1;
        scd.SampleDesc.Quality = 0;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = windowHandle;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        
        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &scd, &swapChain, &device, &featureLevel, &context
        );
        
        if (FAILED(hr)) return false;
        
        // Create render target view
        ID3D11Texture2D* backBuffer = nullptr;
        if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer))) {
            return false;
        }
        
        if (FAILED(device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView))) {
            backBuffer->Release();
            return false;
        }
        
        backBuffer->Release();
        
        context->OMSetRenderTargets(1, &renderTargetView, nullptr);
        
        // Set viewport
        D3D11_VIEWPORT vp = {};
        vp.Width = (float)width;
        vp.Height = (float)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        context->RSSetViewports(1, &vp);
        
        return true;
    }
    
    bool InitializeImGui() {
        // Create ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        // Setup ImGui style
        ImGui::StyleColorsDark();
        
        // Initialize ImGui Win32 backend
        if (!ImGui_ImplWin32_Init(windowHandle)) {
            return false;
        }
        
        // Initialize ImGui DirectX 11 backend
        if (!ImGui_ImplDX11_Init(device, context)) {
            return false;
        }
        
        return true;
    }
    
    void RenderImGuiMenu() {
        if (!menuVisible) return;
        
        ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
        
        if (ImGui::Begin("CS2 Tool", &menuVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
            // Create tabs
            if (ImGui::BeginTabBar("MainTabBar")) {
                // ESP Tab
                if (ImGui::BeginTabItem("ESP")) {
                    ImGui::Checkbox("Enable ESP", &settings.espEnabled);
                    ImGui::Separator();
                    
                    ImGui::Checkbox("Boxes", &settings.espBoxes);
                    ImGui::Checkbox("Health", &settings.espHealth);
                    ImGui::Checkbox("Name", &settings.espName);
                    ImGui::Checkbox("Weapon", &settings.espWeapon);
                    ImGui::Checkbox("Distance", &settings.espDistance);
                    ImGui::Checkbox("Snaplines", &settings.espSnaplines);
                    ImGui::Checkbox("Skeleton", &settings.espSkeleton);
                    
                    ImGui::Separator();
                    ImGui::Text("Colors");
                    
                    float enemyColorArr[4] = {settings.enemyColor.r, settings.enemyColor.g, 
                                             settings.enemyColor.b, settings.enemyColor.a};
                    if (ImGui::ColorEdit4("Enemy", enemyColorArr)) {
                        settings.enemyColor = Color(enemyColorArr[0], enemyColorArr[1], 
                                                   enemyColorArr[2], enemyColorArr[3]);
                    }
                    
                    float teammateColorArr[4] = {settings.teammateColor.r, settings.teammateColor.g,
                                               settings.teammateColor.b, settings.teammateColor.a};
                    if (ImGui::ColorEdit4("Teammate", teammateColorArr)) {
                        settings.teammateColor = Color(teammateColorArr[0], teammateColorArr[1],
                                                     teammateColorArr[2], teammateColorArr[3]);
                    }
                    
                    ImGui::EndTabItem();
                }
                
                // Aimbot Tab
                if (ImGui::BeginTabItem("Aimbot")) {
                    ImGui::Checkbox("Enable Aimbot", &settings.aimbotEnabled);
                    ImGui::Separator();
                    
                    ImGui::SliderFloat("Smoothing", &settings.aimbotSmoothing, 1.0f, 20.0f, "%.1f");
                    ImGui::SliderFloat("FOV", &settings.aimbotFOV, 1.0f, 180.0f, "%.1f");
                    ImGui::SliderFloat("RCS", &settings.aimbotRCS, 0.0f, 2.0f, "%.2f");
                    
                    ImGui::Separator();
                    
                    const char* boneItems[] = {"Head", "Neck", "Chest", "Pelvis", "Feet"};
                    ImGui::Combo("Target Bone", &settings.aimbotBone, boneItems, 5);
                    
                    ImGui::Separator();
                    
                    ImGui::Checkbox("Visibility Check", &settings.aimbotVisibilityCheck);
                    ImGui::Checkbox("Target Lock", &settings.aimbotTargetLock);
                    
                    ImGui::Separator();
                    
                    const char* priorityItems[] = {"FOV", "Distance"};
                    ImGui::Combo("Target Priority", &settings.aimbotPriority, priorityItems, 2);
                    
                    ImGui::EndTabItem();
                }
                
                // Misc Tab
                if (ImGui::BeginTabItem("Misc")) {
                    ImGui::Checkbox("Bunny Hop", &settings.bhopEnabled);
                    ImGui::Checkbox("Radar Hack", &settings.radarEnabled);
                    ImGui::Checkbox("No Flash", &settings.noFlashEnabled);
                    ImGui::Checkbox("Triggerbot", &settings.triggerbotEnabled);
                    
                    ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar();
            }
            
            ImGui::Separator();
            ImGui::Text("Press INSERT to toggle menu");
            ImGui::Text("Press END to unload");
        }
        ImGui::End();
    }
    
    void RenderESP() {
        if (!settings.espEnabled) return;
        
        std::lock_guard<std::mutex> lock(dataMutex);
        
        Vector3 cameraPos = worldToScreen->GetCameraPosition();
        
        for (const auto& player : cachedPlayers) {
            // Skip teammates if desired
            if (player.team == cachedLocalPlayer.team) {
                if (!settings.espBoxes) continue;
            }
            
            // Project position to screen
            Vector3 screenPos;
            if (!worldToScreen->WorldToScreenPoint(player.position, screenPos)) continue;
            
            // Determine color
            Color espColor = (player.team == cachedLocalPlayer.team) ? 
                            settings.teammateColor : settings.enemyColor;
            
            // Draw box
            if (settings.espBoxes) {
                float distance = cameraPos.Distance(player.position);
                float boxHeight = 10000.0f / distance;
                float boxWidth = boxHeight * 0.6f;
                
                // Clamp sizes
                if (boxHeight < 20.0f) boxHeight = 20.0f;
                if (boxHeight > 300.0f) boxHeight = 300.0f;
                
                float boxX = screenPos.x - boxWidth / 2.0f;
                float boxY = screenPos.y - boxHeight / 2.0f;
                
                // Draw box outline (using ImGui for simplicity)
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                drawList->AddRect(ImVec2(boxX, boxY), 
                                 ImVec2(boxX + boxWidth, boxY + boxHeight),
                                 ImColor(espColor.r, espColor.g, espColor.b, espColor.a));
            }
            
            // Draw health bar
            if (settings.espHealth) {
                float healthPercent = (float)player.health / 100.0f;
                float barWidth = 50.0f;
                float barHeight = 5.0f;
                float barX = screenPos.x - barWidth / 2.0f;
                float barY = screenPos.y - 40.0f;
                
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                
                // Background
                drawList->AddRectFilled(ImVec2(barX, barY),
                                       ImVec2(barX + barWidth, barY + barHeight),
                                       ImColor(0, 0, 0, 150));
                
                // Health
                Color healthColor;
                if (healthPercent > 0.5f) {
                    healthColor = Color(2.0f * (1.0f - healthPercent), 1.0f, 0, 1);
                } else {
                    healthColor = Color(1.0f, 2.0f * healthPercent, 0, 1);
                }
                
                drawList->AddRectFilled(ImVec2(barX, barY),
                                       ImVec2(barX + barWidth * healthPercent, barY + barHeight),
                                       ImColor(healthColor.r, healthColor.g, healthColor.b, healthColor.a));
            }
            
            // Draw name
            if (settings.espName) {
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                drawList->AddText(ImVec2(screenPos.x - 20.0f, screenPos.y + 30.0f),
                                 ImColor(1, 1, 1, 1),
                                 player.name.c_str());
            }
            
            // Draw distance
            if (settings.espDistance) {
                char distStr[32];
                sprintf_s(distStr, "%.0fm", player.distance);
                
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                drawList->AddText(ImVec2(screenPos.x + 40.0f, screenPos.y),
                                 ImColor(1, 1, 1, 0.7f),
                                 distStr);
            }
            
            // Draw snaplines
            if (settings.espSnaplines) {
                float centerX = width / 2.0f;
                float centerY = height / 2.0f;
                
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                drawList->AddLine(ImVec2(centerX, centerY),
                                 ImVec2(screenPos.x, screenPos.y),
                                 ImColor(espColor.r, espColor.g, espColor.b, espColor.a));
            }
        }
    }

public:
    ImGuiOverlay() : windowHandle(nullptr), device(nullptr), context(nullptr),
                     swapChain(nullptr), renderTargetView(nullptr),
                     width(1920), height(1080), running(false), menuVisible(false),
                     entityManager(nullptr), worldToScreen(nullptr) {}
    
    ~ImGuiOverlay() {
        Cleanup();
    }
    
    void Cleanup() {
        running = false;
        
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        if (renderTargetView) renderTargetView->Release();
        if (swapChain) swapChain->Release();
        if (context) context->Release();
        if (device) device->Release();
        if (windowHandle) DestroyWindow(windowHandle);
    }
    
    bool Initialize(HWND targetWindow, EntityManager* em, WorldToScreen* wts) {
        entityManager = em;
        worldToScreen = wts;
        
        // Get target window dimensions
        RECT rect;
        GetWindowRect(targetWindow, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
        
        // Register window class
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CS2ImGuiOverlay";
        
        if (!RegisterClassExW(&wc)) {
            return false;
        }
        
        // Create layered, transparent, topmost window
        windowHandle = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            L"CS2ImGuiOverlay",
            L"CS2 Tool",
            WS_POPUP,
            rect.left, rect.top, width, height,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );
        
        if (!windowHandle) return false;
        
        // Set transparency
        SetLayeredWindowAttributes(windowHandle, RGB(0, 0, 0), 255, LWA_ALPHA);
        
        // Use DWM for proper transparency
        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(windowHandle, &margins);
        
        ShowWindow(windowHandle, SW_SHOW);
        UpdateWindow(windowHandle);
        
        // Initialize DirectX
        if (!InitializeDirectX()) {
            return false;
        }
        
        // Initialize ImGui
        if (!InitializeImGui()) {
            return false;
        }
        
        running = true;
        return true;
    }
    
    void UpdateCachedData() {
        std::lock_guard<std::mutex> lock(dataMutex);
        cachedPlayers = entityManager->GetAllPlayers();
        cachedLocalPlayer = entityManager->GetLocalPlayer();
    }
    
    void RenderThread() {
        while (running) {
            // Handle hotkeys
            static bool insertPressed = false;
            static bool endPressed = false;
            
            if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
                if (!insertPressed) {
                    menuVisible = !menuVisible;
                    insertPressed = true;
                }
            } else {
                insertPressed = false;
            }
            
            if (GetAsyncKeyState(VK_END) & 0x8000) {
                if (!endPressed) {
                    running = false;
                    endPressed = true;
                }
            } else {
                endPressed = false;
            }
            
            // Update cached data
            UpdateCachedData();
            
            // Start ImGui frame
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            
            // Clear screen
            float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            context->ClearRenderTargetView(renderTargetView, clearColor);
            
            // Render ESP
            RenderESP();
            
            // Render ImGui menu
            RenderImGuiMenu();
            
            // Render ImGui
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            
            // Present
            swapChain->Present(0, 0); // No VSync for max FPS
            
            // Cap at ~144 FPS
            std::this_thread::sleep_for(std::chrono::milliseconds(7));
        }
    }
    
    void StartRenderThread() {
        std::thread renderThread(&ImGuiOverlay::RenderThread, this);
        renderThread.detach();
    }
    
    bool IsRunning() const {
        return running;
    }
    
    OverlaySettings GetSettings() const {
        return settings;
    }
    
    void SetSettings(const OverlaySettings& newSettings) {
        settings = newSettings;
    }
    
    bool IsMenuVisible() const {
        return menuVisible;
    }
};
