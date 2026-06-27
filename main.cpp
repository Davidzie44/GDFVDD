#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <TlHelp32.h>
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

// CS2 window info from EnumWindows
struct CS2WindowInfo {
    HWND hwnd = NULL;
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    bool valid = false;
};

DWORD g_cs2ProcessId = 0;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != g_cs2ProcessId) return TRUE;

    char className[256] = {};
    GetClassNameA(hwnd, className, sizeof(className));
    if (strcmp(className, "SDL_app") != 0) return TRUE;

    RECT rect;
    if (!GetClientRect(hwnd, &rect)) return TRUE;

    CS2WindowInfo* info = (CS2WindowInfo*)lParam;
    POINT topLeft = { rect.left, rect.top };
    ClientToScreen(hwnd, &topLeft);

    info->hwnd = hwnd;
    info->x = topLeft.x;
    info->y = topLeft.y;
    info->width = rect.right - rect.left;
    info->height = rect.bottom - rect.top;

    if (info->width > 800 && info->height > 600 && info->x >= 0 && info->x < 5000) {
        info->valid = true;
    }

    return TRUE;
}

CS2WindowInfo FindCS2Window() {
    CS2WindowInfo info;
    EnumWindows(EnumWindowsProc, (LPARAM)&info);
    return info;
}

bool IsCS2Foreground(HWND cs2Window) {
    HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    if (foreground == cs2Window) return true;
    HWND parent = GetParent(foreground);
    if (parent == cs2Window) return true;
    return false;
}

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
int aimbotBone = 0;
bool aimbotVisibilityCheck = false;
bool aimbotTargetLock = false;

int aimKey = VK_XBUTTON1;

// DirectX 11 Overlay Window
HWND CreateOverlayWindow(int x, int y, int w, int h) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"CS2Overlay";

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"CS2Overlay", L"CS2 Tool", WS_POPUP,
        x, y, w, h,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );

    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    char msg[256];
    sprintf_s(msg, "Overlay created at (%d, %d) size %dx%d", x, y, w, h);
    Log(msg);
    return hwnd;
}

// Render thread
void RenderThread(HWND overlayWindow, DWORD cs2ProcessId,
                  ProcessMemory* process, EntityManager* entityManager,
                  WorldToScreen* worldToScreen, AimbotAdvanced* aimbot) {
    // Get initial overlay size
    int overlayWidth = GetSystemMetrics(SM_CXSCREEN);
    int overlayHeight = GetSystemMetrics(SM_CYSCREEN);

    CS2WindowInfo cs2Info = FindCS2Window();
    if (cs2Info.valid) {
        overlayWidth = cs2Info.width;
        overlayHeight = cs2Info.height;
    }

    char sizeMsg[128];
    sprintf_s(sizeMsg, "Initial overlay size: %dx%d", overlayWidth, overlayHeight);
    Log(sizeMsg);

    // Initialize DirectX 11
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = overlayWidth;
    scd.BufferDesc.Height = overlayHeight;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = overlayWindow;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

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

    menu.ReadConfig("cs2_tool.ini");

    g_cs2ProcessId = cs2ProcessId;
    int frameCounter = 0;
    HWND cachedCS2Hwnd = cs2Info.hwnd;

    while (running) {
        frameCounter++;

        // Reposition overlay every 100 frames
        if (frameCounter % 100 == 0) {
            CS2WindowInfo info = FindCS2Window();
            if (info.valid) {
                cachedCS2Hwnd = info.hwnd;
                SetWindowPos(overlayWindow, HWND_TOPMOST, info.x, info.y, info.width, info.height, SWP_NOACTIVATE);

                if (info.width != overlayWidth || info.height != overlayHeight) {
                    context->OMSetRenderTargets(0, nullptr, nullptr);
                    if (renderTargetView) { renderTargetView->Release(); renderTargetView = nullptr; }

                    swapChain->ResizeBuffers(0, info.width, info.height, DXGI_FORMAT_UNKNOWN, 0);

                    ID3D11Texture2D* bb = nullptr;
                    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
                    device->CreateRenderTargetView(bb, nullptr, &renderTargetView);
                    bb->Release();

                    context->OMSetRenderTargets(1, &renderTargetView, nullptr);

                    D3D11_VIEWPORT newVp = {0, 0, (float)info.width, (float)info.height, 0, 1};
                    context->RSSetViewports(1, &newVp);

                    overlayWidth = info.width;
                    overlayHeight = info.height;
                    primitives.SetScreenSize(overlayWidth, overlayHeight);

                    char resizeMsg[128];
                    sprintf_s(resizeMsg, "Overlay resized to %dx%d", overlayWidth, overlayHeight);
                    Log(resizeMsg);
                }
            }
        }

        // Check if CS2 is foreground - only toggle visibility on state change
        bool cs2Active = IsCS2Foreground(cachedCS2Hwnd);
        static bool lastCs2Active = false;
        if (cs2Active != lastCs2Active) {
            ShowWindow(overlayWindow, cs2Active ? SW_SHOW : SW_HIDE);
            lastCs2Active = cs2Active;
        }

        // Handle menu input
        menu.SetOpen(menuOpen.load());
        menu.HandleInput();

        // Update settings
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
            const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            context->ClearRenderTargetView(renderTargetView, clearColor);

            {
                std::lock_guard<std::mutex> lock(dataMutex);
                entityManager->Update();
            }

            espRenderer.Render();
            hudRenderer.Render(overlayWidth, overlayHeight, menu.IsOpen());

            if (menu.IsOpen()) {
                menu.Draw(50, 50);
            }

            primitives.Flush();
            swapChain->Present(1, 0);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    menu.WriteConfig("cs2_tool.ini");

    renderTargetView->Release();
    swapChain->Release();
    context->Release();
    device->Release();
    DestroyWindow(overlayWindow);
}

int main() {
    debugLog.open("C:\\cszzzz\\build\\bin\\Release\\debug_log.txt");
    if (debugLog.is_open()) Log("Debug log opened successfully");

    Log("CS2 External Tool - Starting...");

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

    // Get CS2 process ID
    DWORD cs2Pid = 0;
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe = { sizeof(pe) };
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"cs2.exe") == 0) {
                    cs2Pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    if (!cs2Pid) {
        Log("Failed to find cs2.exe process!");
        system("pause");
        return 1;
    }

    g_cs2ProcessId = cs2Pid;

    // Find CS2 window via EnumWindows
    Log("Searching for CS2 window...");
    CS2WindowInfo cs2Info = {};
    for (int i = 0; i < 30 && !cs2Info.valid; i++) {
        cs2Info = FindCS2Window();
        if (!cs2Info.valid) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    HWND cs2Window = cs2Info.hwnd;
    if (!cs2Window) {
        Log("Failed to find CS2 window after 30 attempts!");
        system("pause");
        if (debugLog.is_open()) debugLog.close();
        return 1;
    }

    char foundMsg[256];
    sprintf_s(foundMsg, "Found CS2 window: client area=(%d,%d) size=%dx%d", cs2Info.x, cs2Info.y, cs2Info.width, cs2Info.height);
    Log(foundMsg);

    // Determine overlay size - use CS2 client area if valid, fallback to screen
    int overlayWidth = GetSystemMetrics(SM_CXSCREEN);
    int overlayHeight = GetSystemMetrics(SM_CYSCREEN);
    int overlayX = 0;
    int overlayY = 0;

    if (cs2Info.valid) {
        overlayX = cs2Info.x;
        overlayY = cs2Info.y;
        overlayWidth = cs2Info.width;
        overlayHeight = cs2Info.height;
    }

    // Initialize components
    EntityManager entityManager(process);
    WorldToScreen worldToScreen(process, overlayWidth, overlayHeight);
    AimbotAdvanced aimbot(process, worldToScreen, entityManager);

    // Create overlay window at correct position/size
    HWND overlayWindow = CreateOverlayWindow(overlayX, overlayY, overlayWidth, overlayHeight);

    // Start render thread
    Log("Starting render thread...");
    std::thread renderThread(RenderThread, overlayWindow, cs2Pid,
                           &process, &entityManager, &worldToScreen, &aimbot);

    // Main loop - INSERT only toggles boolean, nothing else
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

        if (GetAsyncKeyState(VK_END) & 0x8000) {
            running = false;
            break;
        }

        // INSERT key - only toggle menu boolean
        if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            if (!insertPressed) {
                menuOpen = !menuOpen;
                insertPressed = true;
            }
        } else {
            insertPressed = false;
        }

        if (aimbotEnabled && (GetAsyncKeyState(aimKey) & 0x8000)) {
            aimbot.Aim();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    renderThread.join();

    Log("Tool unloaded successfully.");
    if (debugLog.is_open()) debugLog.close();
    system("pause");

    return 0;
}
