#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <fstream>
#include <iostream>

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "WorldToScreen.h"
#include "EntityManager.h"
#include "AimbotAdvanced.h"
#include "PrimitivesRenderer.h"
#include "FontRenderer.h"
#include "MenuSystem.h"
#include "ESPRenderer.h"
#include "HUDRenderer.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

// Debug logging
std::ofstream debugLog;

void Log(const char* message) {
    printf("%s\n", message);
    if (debugLog.is_open()) {
        debugLog << message << std::endl;
        debugLog.flush();
    }
}

// Global state
std::atomic<bool> running(true);
std::atomic<bool> menuOpen(false);
std::mutex dataMutex;

// Settings
bool espEnabled = true;
bool espBoxes = true;
bool espHealth = true;
bool espName = true;
bool espWeapon = true;
bool espDistance = true;
bool espSnaplines = false;
bool espHeadDot = true;
bool espShowTeammates = false;

bool aimbotEnabled = false;
float aimbotSmoothing = 2.0f;
float aimbotFOV = 15.0f;
float aimbotRCS = 2.0f;
int aimbotBone = 0; // 0=Head, 1=Neck, 2=Chest, 3=Pelvis, 4=Feet
bool aimbotVisibilityCheck = false;
bool aimbotTargetLock = false;

int aimKey = VK_XBUTTON1;

// Find CS2 window
HWND FindCS2Window() {
    HWND hwnd = nullptr;
    
    while (!hwnd && running) {
        // Try known CS2 window classes
        hwnd = FindWindowW(L"SDL_app", nullptr);
        if (!hwnd) hwnd = FindWindowW(L"Valve001", nullptr);
        
        // Try enumerating all windows to find one with the right title
        if (!hwnd) {
            struct FindData { HWND result; } data = {nullptr};
            EnumWindows([](HWND h, LPARAM lParam) -> BOOL {
                wchar_t title[256] = {};
                GetWindowTextW(h, title, 256);
                if (wcsstr(title, L"Counter-Strike 2") || wcsstr(title, L"CS2")) {
                    wchar_t className[256] = {};
                    GetClassNameW(h, className, 256);
                    // Skip tiny/hidden windows
                    RECT r;
                    if (GetWindowRect(h, &r) && (r.right - r.left) > 200 && (r.bottom - r.top) > 200) {
                        reinterpret_cast<FindData*>(lParam)->result = h;
                        return FALSE;
                    }
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&data));
            hwnd = data.result;
        }
        
        if (!hwnd) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } else {
            // Verify the window is usable
            RECT r;
            GetWindowRect(hwnd, &r);
            wchar_t className[256] = {};
            GetClassNameW(hwnd, className, 256);
            wchar_t title[256] = {};
            GetWindowTextW(hwnd, title, 256);
            
            char logMsg[512];
            sprintf_s(logMsg, "Found window: class='%S' title='%S' rect=(%d,%d,%d,%d) size=%dx%d",
                     className, title, r.left, r.top, r.right, r.bottom,
                     r.right - r.left, r.bottom - r.top);
            Log(logMsg);
            
            // If window is too small or off-screen, try again
            if ((r.right - r.left) < 200 || (r.bottom - r.top) < 200) {
                hwnd = nullptr;
                Log("Window too small, retrying...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    
    return hwnd;
}

// Check if CS2 is the foreground window
bool IsCS2Foreground(HWND cs2Window) {
    HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    if (foreground == cs2Window) return true;
    HWND parent = GetParent(foreground);
    if (parent == cs2Window) return true;
    return false;
}

// Get CS2 window rect - handles borderless fullscreen properly
RECT GetCS2ClientRect(HWND targetWindow) {
    RECT rect;
    GetWindowRect(targetWindow, &rect);
    return rect;
}

// DirectX 11 Overlay Window
HWND CreateOverlayWindow(HWND targetWindow) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    RECT rect = {};
    GetWindowRect(targetWindow, &rect);
    
    int overlayX = rect.left;
    int overlayY = rect.top;
    int overlayWidth = rect.right - rect.left;
    int overlayHeight = rect.bottom - rect.top;
    
    // Detect fullscreen or garbage coordinates from DXGI fullscreen windows
    bool isOffScreen = (overlayX < -100 || overlayY < -100 || 
                       overlayX > screenWidth + 100 || overlayY > screenHeight + 100);
    bool coversScreen = (overlayWidth >= screenWidth - 50 && overlayHeight >= screenHeight - 50);
    bool tooSmall = (overlayWidth < 200 || overlayHeight < 200);
    
    if (isOffScreen || coversScreen || tooSmall) {
        overlayX = 0;
        overlayY = 0;
        overlayWidth = screenWidth;
        overlayHeight = screenHeight;
        Log("Using full screen overlay (detected fullscreen/garbage coords)");
    }
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"CS2Overlay";
    
    RegisterClassExW(&wc);
    
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"CS2Overlay", L"CS2 Tool", WS_POPUP,
        overlayX, overlayY, overlayWidth, overlayHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Use color key: black pixels become fully transparent
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    
    ShowWindow(hwnd, SW_SHOWNA);
    UpdateWindow(hwnd);
    
    char overlayMsg[256];
    sprintf_s(overlayMsg, "Overlay window created: %p at (%d, %d) size %dx%d", hwnd, overlayX, overlayY, overlayWidth, overlayHeight);
    Log(overlayMsg);
    return hwnd;
}

// Render thread
void RenderThread(HWND overlayWindow, HWND targetWindow, 
                  ProcessMemory* process, EntityManager* entityManager,
                  WorldToScreen* worldToScreen, AimbotAdvanced* aimbot) {
    // Wait for overlay window to be fully created
    Sleep(200);
    
    // Get overlay window size
    RECT overlayRect = {};
    GetWindowRect(overlayWindow, &overlayRect);
    int overlayWidth = overlayRect.right - overlayRect.left;
    int overlayHeight = overlayRect.bottom - overlayRect.top;
    
    // Fallback to screen size if window rect is garbage
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (overlayWidth < 100 || overlayHeight < 100 || 
        overlayWidth > screenW + 100 || overlayHeight > screenH + 100) {
        overlayWidth = screenW;
        overlayHeight = screenH;
    }
    
    char sizeMsg[128];
    sprintf_s(sizeMsg, "Overlay size: %dx%d", overlayWidth, overlayHeight);
    Log(sizeMsg);
    
    // Initialize DirectX 11
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = overlayWidth;
    scd.BufferDesc.Height = overlayHeight;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 0;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = overlayWindow;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                   nullptr, 0, D3D11_SDK_VERSION, &scd,
                                   &swapChain, &device, nullptr, &context);
    
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();
    
    context->OMSetRenderTargets(1, &renderTargetView, nullptr);
    
    D3D11_VIEWPORT vp = {0, 0, (float)overlayWidth, (float)overlayHeight, 0, 1};
    context->RSSetViewports(1, &vp);
    
    // Initialize rendering components
    CD3D11Primitives primitives;
    primitives.Initialize(device, context, overlayWidth, overlayHeight);
    
    CD3D11Renderer fontRenderer;
    fontRenderer.Initialize(device, context);
    
    CESPRenderer espRenderer;
    espRenderer.Initialize(&primitives, &fontRenderer, worldToScreen, entityManager);
    
    CHUDRenderer hudRenderer;
    hudRenderer.Initialize(&primitives, &fontRenderer, entityManager, aimbot);
    
    CMenu menu;
    menu.Initialize(&primitives, &fontRenderer);
    
    // Setup menu
    menu.AddTab("ESP");
    menu.AddToggleItem("ESP", "Enabled", &espEnabled, "INSERT");
    menu.AddToggleItem("ESP", "Boxes", &espBoxes);
    menu.AddToggleItem("ESP", "Health Bar", &espHealth);
    menu.AddToggleItem("ESP", "Name", &espName);
    menu.AddToggleItem("ESP", "Weapon", &espWeapon);
    menu.AddToggleItem("ESP", "Distance", &espDistance);
    menu.AddToggleItem("ESP", "Snaplines", &espSnaplines);
    menu.AddToggleItem("ESP", "Head Dot", &espHeadDot);
    menu.AddToggleItem("ESP", "Show Teammates", &espShowTeammates);
    
    menu.AddTab("AIMBOT");
    menu.AddToggleItem("AIMBOT", "Enabled", &aimbotEnabled);
    menu.AddSliderItem("AIMBOT", "Smoothing", &aimbotSmoothing, 1.0f, 20.0f, 0.5f);
    menu.AddSliderItem("AIMBOT", "FOV", &aimbotFOV, 1.0f, 180.0f, 1.0f);
    menu.AddSliderItem("AIMBOT", "RCS", &aimbotRCS, 0.0f, 2.0f, 0.1f);
    std::vector<std::string> boneOptions = {"Head", "Neck", "Chest", "Pelvis", "Feet"};
    menu.AddDropdownItem("AIMBOT", "Target Bone", &aimbotBone, boneOptions);
    menu.AddToggleItem("AIMBOT", "Visibility Check", &aimbotVisibilityCheck);
    menu.AddToggleItem("AIMBOT", "Target Lock", &aimbotTargetLock);
    
    // Load config
    menu.ReadConfig("cs2_tool.ini");
    
    while (running) {
        // Check if CS2 is in the foreground - hide overlay if not
        bool cs2Active = IsCS2Foreground(targetWindow);
        if (cs2Active) {
            // Show overlay (position is already correct from CreateOverlayWindow for fullscreen)
            ShowWindow(overlayWindow, SW_SHOWNA);
        } else {
            // Hide overlay when CS2 is not focused
            ShowWindow(overlayWindow, SW_HIDE);
        }
        
        // Handle menu input
        menu.SetOpen(menuOpen.load());
        menu.HandleInput();
        
        // Update ESP settings from menu
        ESPSettings espSettings;
        espSettings.boxes = espBoxes;
        espSettings.healthBar = espHealth;
        espSettings.name = espName;
        espSettings.weapon = espWeapon;
        espSettings.distance = espDistance;
        espSettings.snaplines = espSnaplines;
        espSettings.headDot = espHeadDot;
        espSettings.showTeammates = espShowTeammates;
        espSettings.aimbotFOV = aimbotFOV;
        espRenderer.SetSettings(espSettings);
        
        // Update aimbot settings
        AimbotSettings aimSettings;
        aimSettings.enabled = aimbotEnabled;
        aimSettings.smoothing = aimbotSmoothing;
        aimSettings.fov = aimbotFOV;
        aimSettings.rcsAmount = aimbotRCS;
        aimSettings.targetBone = (AimBone)aimbotBone;
        aimSettings.visibilityCheck = aimbotVisibilityCheck;
        aimSettings.targetLock = aimbotTargetLock;
        aimbot->SetSettings(aimSettings);
        
        if (cs2Active) {
            // Clear screen - black becomes transparent via color key
            float clearColor[4] = {0, 0, 0, 1};
            context->ClearRenderTargetView(renderTargetView, clearColor);
            
            // Update entity data
            {
                std::lock_guard<std::mutex> lock(dataMutex);
                entityManager->Update();
            }
            
            // Render ESP
            espRenderer.Render();
            
            // Render HUD
            hudRenderer.Render(overlayWidth, overlayHeight, menu.IsOpen());
            
            // Render menu
            if (menu.IsOpen()) {
                menu.Draw(50, 50);
            }
            
            // Flush primitives
            primitives.Flush();
            
            // Present with VSync to reduce flickering
            swapChain->Present(1, 0);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Save config
    menu.WriteConfig("cs2_tool.ini");
    
    // Cleanup
    renderTargetView->Release();
    swapChain->Release();
    context->Release();
    device->Release();
    DestroyWindow(overlayWindow);
}

int main() {
    // Open debug log
    debugLog.open("C:\\cszzzz\\build\\bin\\Release\\debug_log.txt");
    if (debugLog.is_open()) {
        Log("Debug log opened successfully");
    }
    
    Log("CS2 External Tool - Starting...");
    
    // Attach to cs2.exe
    ProcessMemory process;
    try {
        Log("Attaching to cs2.exe...");
        process.Attach(L"cs2.exe");
        Log("Attached successfully!");
    } catch (const std::exception& e) {
        char errorMsg[256];
        sprintf_s(errorMsg, "Failed to attach: %s", e.what());
        Log(errorMsg);
        system("pause");
        if (debugLog.is_open()) debugLog.close();
        return 1;
    }
    
    char baseMsg[256];
    sprintf_s(baseMsg, "Client.dll base: 0x%llX", process.GetClientDllBase());
    Log(baseMsg);
    sprintf_s(baseMsg, "Engine2.dll base: 0x%llX", process.GetEngine2DllBase());
    Log(baseMsg);
    
    // Find CS2 window
    HWND cs2Window = FindCS2Window();
    if (!cs2Window) {
        Log("Failed to find CS2 window!");
        system("pause");
        if (debugLog.is_open()) debugLog.close();
        return 1;
    }
    Log("Found CS2 window!");
    
    // Get CS2 window size for initialization
    RECT initRect = {};
    GetWindowRect(cs2Window, &initRect);
    int initWidth = initRect.right - initRect.left;
    int initHeight = initRect.bottom - initRect.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    bool isOffScreen = (initRect.left < -100 || initRect.top < -100 ||
                       initRect.left > screenW + 100 || initRect.top > screenH + 100);
    bool coversScreen = (initWidth >= screenW - 50 && initHeight >= screenH - 50);
    
    if (initWidth < 200 || initHeight < 200 || isOffScreen || coversScreen) {
        initWidth = screenW;
        initHeight = screenH;
    }
    
    char initMsg[128];
    sprintf_s(initMsg, "CS2 window size: %dx%d", initWidth, initHeight);
    Log(initMsg);
    
    // Initialize components
    EntityManager entityManager(process);
    WorldToScreen worldToScreen(process, initWidth, initHeight);
    AimbotAdvanced aimbot(process, worldToScreen, entityManager);
    
    // Create overlay window
    HWND overlayWindow = CreateOverlayWindow(cs2Window);
    
    // Start render thread
    Log("Starting render thread...");
    std::thread renderThread(RenderThread, overlayWindow, cs2Window, 
                           &process, &entityManager, &worldToScreen, &aimbot);
    
    // Main loop
    Log("Main loop started. Press INSERT to toggle menu, END to unload.");
    
    static bool insertPressed = false;
    
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;
        
        // Check END key
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            running = false;
            break;
        }
        
        // Check INSERT key - toggle menu
        if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            if (!insertPressed) {
                menuOpen = !menuOpen;
                insertPressed = true;
                char menuMsg[64];
                sprintf_s(menuMsg, "Menu %s", menuOpen ? "opened" : "closed");
                Log(menuMsg);
            }
        } else {
            insertPressed = false;
        }
        
        // Aimbot logic
        if (aimbotEnabled && (GetAsyncKeyState(aimKey) & 0x8000)) {
            aimbot.Aim();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Wait for render thread
    renderThread.join();
    
    Log("Tool unloaded successfully.");
    if (debugLog.is_open()) debugLog.close();
    system("pause");
    
    return 0;
}
