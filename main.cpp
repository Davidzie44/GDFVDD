#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <TlHelp32.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <cmath>
#include <fstream>
#include <exception>
#include <cstdio>
#include <cstdarg>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "WorldToScreen.h"
#include "EntityManager.h"
#include "AimbotAdvanced.h"
#include "Offsets.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

std::ofstream g_log;
std::atomic<bool> running(true);
std::atomic<bool> menuOpen(false);

void Log(const char* msg) {
    if (g_log.is_open()) {
        g_log << msg << std::endl;
        g_log.flush();
    }
}

void LogFmt(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log(buf);
}

LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    LogFmt("CRASH: Exception code 0x%08X at address %p",
        ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        LogFmt("  Access violation: reading/writing address %p",
            (void*)ep->ExceptionRecord->ExceptionInformation[1]);
    }
    g_log.flush();
    return EXCEPTION_EXECUTE_HANDLER;
}
std::mutex dataMutex;

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

    POINT topLeft = { rect.left, rect.top };
    ClientToScreen(hwnd, &topLeft);

    HWND* outHwnd = (HWND*)lParam;
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    if (w > 800 && h > 600 && topLeft.x >= 0 && topLeft.x < 5000) {
        *outHwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND FindCS2Window() {
    HWND hwnd = NULL;
    EnumWindows(EnumWindowsProc, (LPARAM)&hwnd);
    return hwnd;
}

bool IsCS2Foreground() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD fgPid = 0;
    GetWindowThreadProcessId(fg, &fgPid);
    return (fgPid == g_cs2ProcessId);
}

struct CS2WindowRect {
    int x, y, w, h;
};

CS2WindowRect GetCS2ClientRect(HWND cs2Hwnd) {
    RECT rect;
    GetClientRect(cs2Hwnd, &rect);
    POINT pt = { 0, 0 };
    ClientToScreen(cs2Hwnd, &pt);
    return { pt.x, pt.y, rect.right - rect.left, rect.bottom - rect.top };
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
float aimbotSmoothing = 1.0f;
float aimbotFOV = 30.0f;
float aimbotRCS = 2.0f;
int aimbotBone = 0;
bool aimbotVisibilityCheck = false;
bool aimbotTargetLock = false;
int aimKey = VK_RBUTTON;

bool triggerbotEnabled = false;
bool triggerbotHeadOnly = false;
bool rapidFireEnabled = false;
float rapidFireSpeed = 8.0f;

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_DESTROY:
        running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void RenderThread(HWND overlayWindow, DWORD cs2ProcessId,
                  ProcessMemory* process, EntityManager* entityManager,
                  WorldToScreen* worldToScreen, AimbotAdvanced* aimbot) {

    Log("RenderThread: Starting");
    HWND cs2Hwnd = FindCS2Window();
    if (!cs2Hwnd) {
        Log("RenderThread: FATAL - CS2 window gone");
        running = false;
        return;
    }

    CS2WindowRect cs2Rect = GetCS2ClientRect(cs2Hwnd);
    int overlayWidth = cs2Rect.w;
    int overlayHeight = cs2Rect.h;
    LogFmt("RenderThread: CS2 client %dx%d", overlayWidth, overlayHeight);

    // D3D11 setup
    Log("RenderThread: Creating D3D11 device and swap chain");
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = overlayWindow;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, 2, D3D11_SDK_VERSION, &scd, &swapChain, &device, &featureLevel, &context);
    if (FAILED(hr)) {
        LogFmt("RenderThread: FATAL - D3D11CreateDeviceAndSwapChain failed: 0x%08X", (unsigned)hr);
        running = false;
        return;
    }
    LogFmt("RenderThread: D3D11 device created, feature level=%u", (unsigned)featureLevel);

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();
    Log("RenderThread: Render target view created");

    // ImGui init
    Log("RenderThread: Initializing ImGui");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(overlayWindow);
    ImGui_ImplDX11_Init(device, context);
    Log("RenderThread: ImGui initialized OK");

    g_cs2ProcessId = cs2ProcessId;

    bool prevInsert = false;
    int frameCount = 0;

    Log("RenderThread: Entering render loop");

    while (running) {
        frameCount++;
        if (frameCount <= 3 || frameCount % 300 == 0) {
            LogFmt("RenderThread: Frame %d start", frameCount);
        }

        try {

        // INSERT key toggle (main thread pumps messages, render thread reads keys)
        bool curInsert = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (curInsert && !prevInsert) {
            menuOpen = !menuOpen;
            LONG ex = GetWindowLong(overlayWindow, GWL_EXSTYLE);
            if (menuOpen)
                SetWindowLong(overlayWindow, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
            else
                SetWindowLong(overlayWindow, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
        }
        prevInsert = curInsert;

        if (GetAsyncKeyState(VK_END) & 0x8000) {
            running = false;
            break;
        }

        // Track game window - always update position, only hide when menu closed
        {
            CS2WindowRect r = GetCS2ClientRect(cs2Hwnd);
            bool cs2Active = IsCS2Foreground();

            if (cs2Active && r.w > 100 && r.h > 100) {
                SetWindowPos(overlayWindow, HWND_TOPMOST, r.x, r.y, r.w, r.h, SWP_NOACTIVATE);

                if (r.w != overlayWidth || r.h != overlayHeight) {
                    overlayWidth = r.w;
                    overlayHeight = r.h;
                    if (rtv) { rtv->Release(); rtv = nullptr; }
                    swapChain->ResizeBuffers(0, overlayWidth, overlayHeight, DXGI_FORMAT_UNKNOWN, 0);
                    ID3D11Texture2D* bb = nullptr;
                    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
                    device->CreateRenderTargetView(bb, nullptr, &rtv);
                    bb->Release();
                    worldToScreen->SetScreenSize(overlayWidth, overlayHeight);
                }
            }

            if (cs2Active || menuOpen.load()) {
                ShowWindow(overlayWindow, SW_SHOW);
            } else {
                ShowWindow(overlayWindow, SW_HIDE);
            }
        }

        if (frameCount <= 3 || frameCount % 300 == 0) {
            LogFmt("  Frame %d: calling entityManager->Update()", frameCount);
        }

        // Update entity data
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            entityManager->Update();
        }

        if (frameCount <= 3 || frameCount % 300 == 0) {
            LogFmt("  Frame %d: ImGui new frame + draw", frameCount);
        }

        // ImGui new frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // --- Draw ESP ---
        if (espEnabled) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();

            // Draw FOV circle
            if (aimbotEnabled) {
                float centerX = overlayWidth / 2.0f;
                float centerY = overlayHeight / 2.0f;
                float fovRadius = std::tan(aimbotFOV * 3.14159f / 180.0f) * (overlayWidth / 2.0f);
                if (fovRadius < 5.0f) fovRadius = 5.0f;
                drawList->AddCircle(ImVec2(centerX, centerY), fovRadius, IM_COL32(255, 255, 255, 120), 64, 1.5f);
            }

            const PlayerData& localPlayer = entityManager->GetLocalPlayer();
            if (frameCount <= 3 || frameCount % 300 == 0) {
                LogFmt("  ESP: local alive=%d team=%d hp=%d pos=(%.0f,%.0f,%.0f) players=%d",
                    localPlayer.isAlive, localPlayer.team, localPlayer.health,
                    localPlayer.position.x, localPlayer.position.y, localPlayer.position.z,
                    (int)entityManager->GetAllPlayers().size());
            }
            if (localPlayer.isAlive && localPlayer.position.Length() > 1.0f) {
                auto players = entityManager->GetAllPlayers();
                for (const auto& player : players) {
                    if (player.team == localPlayer.team && !espShowTeammates) continue;

                    bool isEnemy = (player.team != localPlayer.team);
                    ImU32 espColor = isEnemy ? IM_COL32(255, 50, 50, 255) : IM_COL32(50, 255, 50, 255);

                    Vector3 screenHead, screenFoot;
                    bool footOk = worldToScreen->WorldToScreenPoint(player.position, screenFoot);

                    Vector3 estimatedHead(player.position.x, player.position.y, player.position.z + 72.0f);
                    bool headOk = worldToScreen->WorldToScreenPoint(estimatedHead, screenHead);
                    if (!headOk) {
                        screenHead = screenFoot;
                    }

                    if (!footOk && !headOk) continue;

                    float boxH = std::abs(screenHead.y - screenFoot.y);
                    if (boxH < 20.0f) boxH = 20.0f;
                    float boxW = boxH * 0.6f;
                    float boxX = screenHead.x - boxW / 2.0f;
                    float boxY = screenHead.y;

                    if (espBoxes) {
                        drawList->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), espColor);
                    }

                    if (espHealth) {
                        float hp = (float)player.health / 100.0f;
                        ImU32 hpColor = hp > 0.5f ? IM_COL32(255, 255 * (1.0f - (hp - 0.5f) * 2.0f), 0, 255)
                                                   : IM_COL32(255, 255 * hp * 2.0f, 0, 255);
                        drawList->AddRectFilled(ImVec2(boxX - 5, boxY), ImVec2(boxX - 1, boxY + boxH), IM_COL32(0, 0, 0, 200));
                        drawList->AddRectFilled(ImVec2(boxX - 5, boxY + boxH * (1.0f - hp)), ImVec2(boxX - 1, boxY + boxH), hpColor);
                    }

                    if (espName && !player.name.empty()) {
                        ImVec2 textSize = ImGui::CalcTextSize(player.name.c_str());
                        drawList->AddText(ImVec2(boxX + boxW / 2 - textSize.x / 2, boxY - textSize.y - 2), IM_COL32(255, 255, 255, 255), player.name.c_str());
                    }

                    if (espDistance) {
                        char distStr[32];
                        sprintf_s(distStr, "%.0fm", player.distance);
                        ImVec2 textSize = ImGui::CalcTextSize(distStr);
                        drawList->AddText(ImVec2(boxX + boxW / 2 - textSize.x / 2, boxY + boxH + 2), IM_COL32(200, 200, 200, 255), distStr);
                    }

                    if (espSnaplines) {
                        ImVec2 center(overlayWidth / 2.0f, overlayHeight);
                        drawList->AddLine(center, ImVec2(screenFoot.x, screenFoot.y), espColor, 1.0f);
                    }

                    if (espHeadDot) {
                        drawList->AddCircleFilled(ImVec2(screenHead.x, screenHead.y), 3.0f, IM_COL32(255, 0, 0, 255), 16);
                    }
                }
            }
        }

        // --- Draw Menu ---
        if (menuOpen) {
            ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_Once);
            ImGui::Begin("CS2 Tool", nullptr,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

            if (ImGui::BeginTabBar("Tabs")) {
                if (ImGui::BeginTabItem("ESP")) {
                    ImGui::Checkbox("Enabled", &espEnabled);
                    ImGui::Checkbox("Boxes", &espBoxes);
                    ImGui::Checkbox("Health Bar", &espHealth);
                    ImGui::Checkbox("Name", &espName);
                    ImGui::Checkbox("Weapon", &espWeapon);
                    ImGui::Checkbox("Distance", &espDistance);
                    ImGui::Checkbox("Snaplines", &espSnaplines);
                    ImGui::Checkbox("Head Dot", &espHeadDot);
                    ImGui::Checkbox("Show Teammates", &espShowTeammates);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Aimbot")) {
                    ImGui::Checkbox("Enabled", &aimbotEnabled);
                    ImGui::SliderFloat("Smoothing", &aimbotSmoothing, 1.0f, 20.0f);
                    ImGui::SliderFloat("FOV", &aimbotFOV, 1.0f, 180.0f);
                    ImGui::SliderFloat("RCS", &aimbotRCS, 0.0f, 2.0f);
                    const char* bones[] = { "Head", "Neck", "Chest", "Pelvis", "Feet" };
                    ImGui::Combo("Target Bone", &aimbotBone, bones, IM_ARRAYSIZE(bones));
                    ImGui::Checkbox("Visibility Check", &aimbotVisibilityCheck);
                    ImGui::Checkbox("Target Lock (Follow)", &aimbotTargetLock);
                    ImGui::Separator();
                    ImGui::Checkbox("Triggerbot", &triggerbotEnabled);
                    ImGui::Checkbox("Triggerbot Head Only", &triggerbotHeadOnly);
                    ImGui::Separator();
                    ImGui::Checkbox("Rapid Fire", &rapidFireEnabled);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::Separator();
            ImGui::Text("Press INSERT to toggle menu");
            ImGui::Separator();
            if (ImGui::Button("Detach & Exit", ImVec2(-1, 0))) {
                running = false;
            }
            ImGui::End();
        }

        // Update settings
        AimbotSettings aimSettings;
        aimSettings.enabled = aimbotEnabled;
        aimSettings.smoothing = aimbotSmoothing;
        aimSettings.fov = aimbotFOV;
        aimSettings.rcsAmount = aimbotRCS;
        aimSettings.targetBone = (AimBone)aimbotBone;
        aimSettings.visibilityCheck = aimbotVisibilityCheck;
        aimSettings.targetLock = aimbotTargetLock;
        aimSettings.triggerbotEnabled = triggerbotEnabled;
        aimSettings.triggerbotHeadOnly = triggerbotHeadOnly;
        aimSettings.rapidFireEnabled = rapidFireEnabled;
        aimSettings.rapidFireSpeed = rapidFireSpeed;
        aimbot->SetSettings(aimSettings);

        // Run aimbot when aim key is held (right mouse button)
        if (aimbotEnabled && (GetAsyncKeyState(aimKey) & 0x8000)) {
            aimbot->Aim();
        } else if (!aimbotTargetLock) {
            aimbot->ResetTarget();
        }

        // Run triggerbot every frame when enabled
        if (triggerbotEnabled) {
            aimbot->Triggerbot();
        }

        // Run rapid fire every frame when enabled
        if (rapidFireEnabled) {
            aimbot->RapidFire();
        }

        // Render
        if (frameCount <= 3 || frameCount % 300 == 0) {
            LogFmt("  Frame %d: ImGui::Render + Present", frameCount);
        }
        ImGui::Render();
        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        context->OMSetRenderTargets(1, &rtv, nullptr);
        context->ClearRenderTargetView(rtv, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swapChain->Present(1, 0);
        if (frameCount <= 3 || frameCount % 300 == 0) {
            LogFmt("  Frame %d: Present OK, frame done", frameCount);
        }

        } catch (const std::exception& e) {
            LogFmt("C++ EXCEPTION in render loop: %s", e.what());
            break;
        } catch (...) {
            LogFmt("UNKNOWN EXCEPTION in render loop at frame %d", frameCount);
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (rtv) rtv->Release();
    swapChain->Release();
    context->Release();
    device->Release();
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetUnhandledExceptionFilter(CrashHandler);

    g_log.open("crash.log", std::ios::out);
    Log("=== CS2 External Tool Starting ===");
    Log("Step 1: ProcessMemory attach");

    ProcessMemory process;
    try {
        process.Attach(L"cs2.exe");
        Log("Attached to cs2.exe OK");
    } catch (const std::exception& e) {
        LogFmt("FATAL: Attach failed: %s", e.what());
        MessageBoxA(nullptr, "Failed to attach to cs2.exe. See crash.log", "Error", MB_ICONERROR);
        return 1;
    }

    Log("Step 2: Find cs2.exe PID");
    DWORD cs2Pid = 0;
    {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) {
            Log("FATAL: CreateToolhelp32Snapshot failed");
            return 1;
        }
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
        Log("FATAL: cs2.exe PID not found");
        MessageBoxA(nullptr, "cs2.exe not found. See crash.log", "Error", MB_ICONERROR);
        return 1;
    }
    LogFmt("cs2.exe PID = %lu", cs2Pid);
    g_cs2ProcessId = cs2Pid;

    Log("Step 3: Find CS2 window (EnumWindows)");
    HWND cs2Hwnd = NULL;
    for (int i = 0; i < 30 && !cs2Hwnd; i++) {
        cs2Hwnd = FindCS2Window();
        if (!cs2Hwnd) {
            LogFmt("  Attempt %d/30 - no window yet", i + 1);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    if (!cs2Hwnd) {
        Log("FATAL: CS2 window not found after 30 attempts");
        MessageBoxA(nullptr, "Failed to find CS2 window. See crash.log", "Error", MB_ICONERROR);
        return 1;
    }
    LogFmt("CS2 HWND = %p", (void*)cs2Hwnd);

    CS2WindowRect cs2Rect = GetCS2ClientRect(cs2Hwnd);
    LogFmt("CS2 client area: (%d, %d) %dx%d", cs2Rect.x, cs2Rect.y, cs2Rect.w, cs2Rect.h);

    Log("Step 4: Register window class");
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, OverlayWndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"CS2Overlay", NULL };
    ATOM atom = RegisterClassExW(&wc);
    if (!atom) {
        LogFmt("FATAL: RegisterClassExW failed, GetLastError=%lu", GetLastError());
        return 1;
    }

    Log("Step 5: Create overlay window");
    HWND overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        L"CS2Overlay", L"CS2 Tool", WS_POPUP,
        cs2Rect.x, cs2Rect.y, cs2Rect.w, cs2Rect.h,
        nullptr, nullptr, GetModuleHandle(NULL), nullptr
    );
    if (!overlayWindow) {
        LogFmt("FATAL: CreateWindowExW failed, GetLastError=%lu", GetLastError());
        return 1;
    }
    LogFmt("Overlay HWND = %p", (void*)overlayWindow);

    Log("Step 6: SetLayeredWindowAttributes + DwmExtendFrameIntoClientArea");
    SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 0, LWA_COLORKEY);
    MARGINS m = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(overlayWindow, &m);

    Log("Step 7: ShowWindow + UpdateWindow");
    ShowWindow(overlayWindow, SW_SHOWDEFAULT);
    UpdateWindow(overlayWindow);

    Log("Step 8: Create EntityManager, WorldToScreen, AimbotAdvanced");
    EntityManager entityManager(process);
    WorldToScreen worldToScreen(process, cs2Rect.w, cs2Rect.h);
    AimbotAdvanced aimbot(process, worldToScreen, entityManager);
    Log("All components created OK");

    Log("Step 9: Start render thread");
    std::thread renderThread(RenderThread, overlayWindow, cs2Pid,
        &process, &entityManager, &worldToScreen, &aimbot);
    Log("Render thread started");

    Log("Step 10: Entering main loop");
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Log("Shutting down...");
    renderThread.join();
    Log("Done.");
    g_log.close();
    return 0;
}
