#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <TlHelp32.h>
#include <random>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

// ===================================================================
// MATH TYPES
// ===================================================================
struct Vector2 { float x,y;
    Vector2():x(0),y(0){} Vector2(float a,float b):x(a),y(b){}
    float Dist(Vector2 o){float dx=x-o.x,dy=y-o.y;return sqrtf(dx*dx+dy*dy);}
};
struct Vector3 { float x,y,z;
    Vector3():x(0),y(0),z(0){} Vector3(float a,float b,float c):x(a),y(b),z(c){}
    Vector3 operator-(Vector3 o){return{x-o.x,y-o.y,z-o.z};}
    float Len(){return sqrtf(x*x+y*y+z*z);}
};

struct Entity {
    uintptr_t addr=0; Vector3 pos,head;
    float hp=0,mhp=100; int team=0; char name[64]={};
    bool alive=false; float dist=0;
    Vector2 sp,sh,boxTL,boxBR;
};

struct Config {
    bool esp=true,espBox=true,espSnap=true,espHealth=true,espTeamCheck=true,espName=true;
    float espMaxDist=5000.0f;
    bool aim=true; float aimFov=6.0f,aimSmooth=4.0f; int aimKey=1; // 0=LM 1=RM 2=MM
    bool trig=false; float trigFov=12.0f; int trigMin=15,trigMax=35;
    bool wb=false; float penMult=20.0f;
    bool menu=true;
    bool enableHook=true; // Enable with delayed hook
}cfg;

const char* aimKeys[] = {"Left Mouse", "Right Mouse", "Middle Mouse", "X1 Mouse", "X2 Mouse"};
int aimKeyCodes[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};

// ===================================================================
// MEMORY - same pattern scanning as before
// ===================================================================
class Memory {
    HANDLE p; uintptr_t base=0,gPtr=0,lpPtr=0,plPtr=0,vmAddr=0;
    uint32_t oPos=0x80,oHead=0x90,oHp=0xC0,oMhp=0xC4,oTeam=0xD0,oName=0x100,oAng=0x1A0;
    uint8_t colOrig[32]; int colSize=0; bool colSaved=false;
    uintptr_t RP(uintptr_t a){return R<uintptr_t>(a);}
public:
    uintptr_t colAddr=0; // Made public for menu display
    Memory():p(GetCurrentProcess()){}
    uintptr_t GetMod(const wchar_t* n){
        HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());
        MODULEENTRY32W e={sizeof(MODULEENTRY32W)};
        if(Module32FirstW(s,&e)) do if(_wcsicmp(e.szModule,n)==0){CloseHandle(s);return(uintptr_t)e.modBaseAddr;}while(Module32NextW(s,&e));
        CloseHandle(s);return 0;
    }
    template<typename T>T R(uintptr_t a){T v={};ReadProcessMemory(p,(LPCVOID)a,&v,sizeof(T),NULL);return v;}
    void RB(uintptr_t a,void* b,size_t sz){ReadProcessMemory(p,(LPCVOID)a,b,sz,NULL);}
    template<typename T>void W(uintptr_t a,T v){WriteProcessMemory(p,(LPVOID)a,&v,sizeof(T),NULL);}
    void WB(uintptr_t a,void* b,size_t sz){DWORD old;VirtualProtect((LPVOID)a,sz,PAGE_EXECUTE_READWRITE,&old);WriteProcessMemory(p,(LPVOID)a,b,sz,NULL);VirtualProtect((LPVOID)a,sz,old,&old);}

    uintptr_t FindSig(uint8_t* pat,char* mask){
        if(!base)return 0;
        auto dos=(IMAGE_DOS_HEADER*)base;
        auto nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);
        auto sec=IMAGE_FIRST_SECTION(nt);
        for(WORD i=0;i<nt->FileHeader.NumberOfSections;i++){
            char sn[9]={};memcpy(sn,sec[i].Name,8);
            if(strcmp(sn,".text")==0){
                uintptr_t st=base+sec[i].VirtualAddress;
                size_t sz=sec[i].Misc.VirtualSize;
                for(size_t j=0;j<sz-strlen(mask);j++){
                    bool ok=true;
                    for(size_t k=0;k<strlen(mask);k++)
                        if(mask[k]=='x'&&((uint8_t*)(st+j))[k]!=pat[k]){ok=false;break;}
                    if(ok)return st+j;
                }
            }
        }
        return 0;
    }
    uintptr_t RIP(uintptr_t a,int o=3,int l=7){return a?a+l+*(int32_t*)(a+o):0;}

    bool ScanAll(){
        base=GetMod(L"aces.exe");if(!base)base=GetMod(L"launcher.exe");
        if(!base){printf("[!] Game module not found\n");return false;}
        printf("[+] Base: 0x%llX\n",base);

        uint8_t p1[]={0x48,0x8D,0x0D,0,0,0,0,0xE8};
        uint8_t p2[]={0x48,0x8B,0x0D,0,0,0,0,0x48,0x85,0xC9,0x74};
        uint8_t p3[]={0x48,0x8B,0x3D,0,0,0,0,0x48,0x85,0xFF,0x74};
        uint8_t p4[]={0x0F,0x11,0x05,0,0,0,0,0x0F,0x11,0x0D};
        uint8_t pCol[]={0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9};
        uintptr_t a;
        a=FindSig(p1,"xxx????x");if(a){gPtr=RIP(a);printf("[+] cGame: 0x%llX\n",gPtr);}
        a=FindSig(p2,"xxx????xxxx");if(a){lpPtr=RIP(a);printf("[+] cLocalPlayer: 0x%llX\n",lpPtr);}
        a=FindSig(p3,"xxx????xxxx");if(a){plPtr=RIP(a);printf("[+] cPlayerList: 0x%llX\n",plPtr);}
        a=FindSig(p4,"xxx????xxx");if(a){vmAddr=RIP(a);printf("[+] ViewMatrix: 0x%llX\n",vmAddr);}
        a=FindSig(pCol,"xxxxxxxxx");if(a){colAddr=a;colSize=5;printf("[+] Collision func: 0x%llX\n",colAddr);}
        if(!colAddr){uint8_t pC2[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20};
            a=FindSig(pC2,"xxxxxxxxxxxxxxx");if(a){colAddr=a;colSize=5;printf("[+] Collision func2: 0x%llX\n",colAddr);}}

        // Auto-detect entity fields
        if(plPtr){uintptr_t g=RP(base+gPtr);if(g){uintptr_t l=RP(g+plPtr);if(l){uintptr_t e=RP(l);if(e){
            uint8_t b[0x400];RB(e,b,0x400);
            for(int i=0;i<0x200;i+=4)if(*(float*)&b[i]==100.0f){oHp=i;break;}
            for(int i=0;i<0x200;i+=4){int v=*(int*)&b[i];if(v>=0&&v<=5){oTeam=i;break;}}
            for(int i=0;i<0x200;i+=4){
                float f1=*(float*)&b[i],f2=*(float*)&b[i+4],f3=*(float*)&b[i+8];
                if(fabs(f1)>50&&fabs(f2)>50&&fabs(f3)>0&&fabs(f1)<100000){oPos=i;break;}
            }
            for(int i=0;i<0x200;i++)if(b[i]>='A'&&b[i]<='Z'&&b[i+1]>='a'){oName=i;break;}
        }}}}
        printf("[*] pos:0x%X hp:0x%X team:0x%X name:0x%X\n",oPos,oHp,oTeam,oName);
        return gPtr&&lpPtr&&plPtr&&vmAddr;
    }

    Entity GetLP(){
        Entity e;uintptr_t g=RP(base+gPtr);if(!g)return e;
        e.addr=RP(g+lpPtr);if(!e.addr)return e;
        e.pos=R<Vector3>(e.addr+oPos);e.head=R<Vector3>(e.addr+oHead);
        e.hp=R<float>(e.addr+oHp);e.mhp=R<float>(e.addr+oMhp);
        e.team=R<int>(e.addr+oTeam);RB(e.addr+oName,e.name,64);e.alive=e.hp>0.1f;
        return e;
    }

    std::vector<Entity> GetEnts(){
        std::vector<Entity> v;uintptr_t g=RP(base+gPtr);if(!g)return v;
        uintptr_t l=RP(g+plPtr);if(!l)return v;
        int c=R<int>(l+0x8);if(c>64||c<0)c=50;
        for(int i=0;i<c;i++){uintptr_t a=RP(l+0x10+i*8);if(!a)continue;
            Entity e;e.addr=a;e.pos=R<Vector3>(a+oPos);e.head=R<Vector3>(a+oHead);
            e.hp=R<float>(a+oHp);e.mhp=R<float>(a+oMhp);e.team=R<int>(a+oTeam);
            RB(a+oName,e.name,64);e.alive=e.hp>0.1f;v.push_back(e);}
        return v;
    }

    XMMATRIX GetVM(){XMMATRIX m;RB(vmAddr,&m,sizeof(m));return m;}

    bool W2S(Vector3 w,Vector2&s,int sw=1920,int sh=1080){
        XMMATRIX vm=GetVM();XMVECTOR v=XMLoadFloat3((XMFLOAT3*)&w);
        XMVECTOR t=XMVector3Transform(v,vm);float wv=XMVectorGetW(t);
        if(wv<0.01f)return false;
        s.x=(sw/2.0f)*(1+XMVectorGetX(t)/wv);s.y=(sh/2.0f)*(1-XMVectorGetY(t)/wv);
        return true;
    }

    Vector2 CalcAng(Vector3 src,Vector3 dst){
        Vector3 d=dst-src;float l=d.Len();if(l<0.01f)return{0,0};
        return{-atan2f(d.y,d.x)*(180/XM_PI),asinf(d.z/l)*(180/XM_PI)};
    }

    void SetAng(Vector2 a){
        uintptr_t g=RP(base+gPtr);if(!g)return;uintptr_t lp=RP(g+lpPtr);if(!lp)return;
        W<float>(lp+oAng,a.y);W<float>(lp+oAng+4,a.x);
    }

    void EnableWallbang(){
        if(!colAddr){printf("[!] No collision func found\n");return;}
        if(!colSaved){RB(colAddr,colOrig,colSize);colSaved=true;}
        uint8_t nops[32];memset(nops,0x90,colSize);WB(colAddr,nops,colSize);
        printf("[+] Wallbang ON\n");
    }
    void DisableWallbang(){if(colAddr&&colSaved)WB(colAddr,colOrig,colSize);printf("[-] Wallbang OFF\n");}
}gMem;

// ===================================================================
// GLOBAL WINDOW PROC FOR IMGUI
// ===================================================================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static WNDPROC oWndProc = NULL;
HWND g_hWindow = NULL;

LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    if(cfg.menu && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;
    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

// ===================================================================
// D3D11 PRESENT HOOK + IMGUI
// ===================================================================
class D3D11Hook {
    ID3D11Device*dev=nullptr;ID3D11DeviceContext*ctx=nullptr;
    IDXGISwapChain*sc=nullptr;ID3D11RenderTargetView*rtv=nullptr;
    bool init=false; int sw=1920,sh=1080;
    typedef HRESULT(__stdcall*P_t)(IDXGISwapChain*,UINT,UINT);P_t orig=nullptr;
public:
    static D3D11Hook*inst;
    static HRESULT __stdcall Hook(IDXGISwapChain*c,UINT s,UINT f){
        return inst->OnPresent(c,s,f);
    }

    HRESULT OnPresent(IDXGISwapChain*c,UINT s,UINT f){
        if(!init){
            sc=c;sc->GetDevice(__uuidof(ID3D11Device),(void**)&dev);dev->GetImmediateContext(&ctx);
            ID3D11Texture2D*bb=nullptr;sc->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&bb);
            dev->CreateRenderTargetView(bb,nullptr,&rtv);bb->Release();
            DXGI_SWAP_CHAIN_DESC d;sc->GetDesc(&d);sw=d.BufferDesc.Width;sh=d.BufferDesc.Height;

            // Get game window for ImGui
            g_hWindow = d.OutputWindow;
            oWndProc = (WNDPROC)SetWindowLongPtrW(g_hWindow, GWLP_WNDPROC, (LONG_PTR)WndProcHook);

            // Init ImGui
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            ImGui_ImplWin32_Init(g_hWindow);
            ImGui_ImplDX11_Init(dev, ctx);
            ImGui::StyleColorsDark();

            init=true; printf("[+] ImGui initialized\n");
        }

        // Draw ImGui
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ====== OVERLAY DRAWING ======
        DoESP();
        if(cfg.menu) DrawMenu();

        ImGui::EndFrame();
        ImGui::Render();
        ctx->OMSetRenderTargets(1, &rtv, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return orig(c,s,f);
    }

    // ====== ESP (uses ImGui DrawList for text) ======
    void DoESP(){
        if(!cfg.esp)return;
        auto lp=gMem.GetLP();if(!lp.addr)return;
        auto es=gMem.GetEnts();
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        for(auto&e:es){
            if(e.addr==lp.addr||!e.alive)continue;
            if(cfg.espTeamCheck&&e.team==lp.team)continue;
            e.dist=(e.pos-lp.pos).Len();
            if(e.dist>cfg.espMaxDist)continue;
            if(!gMem.W2S(e.pos,e.sp,sw,sh)||!gMem.W2S(e.head,e.sh,sw,sh))continue;

            float h=fabs(e.sp.y-e.sh.y);if(h<5)h=e.dist*0.05f;
            float w=h*0.6f;if(w<6)w=6;if(h<6)h=6;
            float bx=e.sp.x-w/2,by=e.sh.y;

            ImU32 color = (e.team==lp.team) ? IM_COL32(0,255,0,180) : IM_COL32(255,0,0,180);
            float alpha = (e.dist>1000) ? max(0.2f,1-e.dist/5000) : 1.0f;
            ImU32 colFade = (e.team==lp.team) ? IM_COL32(0,255,0,(int)(255*alpha)) : IM_COL32(255,0,0,(int)(255*alpha));

            if(cfg.espBox) dl->AddRect(ImVec2(bx,by), ImVec2(bx+w,by+h), colFade);
            if(cfg.espSnap) dl->AddLine(ImVec2(sw/2.0f,sh), ImVec2(e.sp.x,e.sp.y), IM_COL32(255,255,255,60));
            if(cfg.espHealth){
                float p=e.hp/e.mhp;if(p<0)p=0;
                dl->AddRectFilled(ImVec2(bx-6,by), ImVec2(bx-3,by+h), IM_COL32(0,0,0,150));
                ImU32 hc = (p>0.5f)?IM_COL32(0,255,0,255):((p>0.25f)?IM_COL32(255,255,0,255):IM_COL32(255,0,0,255));
                dl->AddRectFilled(ImVec2(bx-6,by+h*(1-p)), ImVec2(bx-3,by+h), hc);
            }
            if(cfg.espName) dl->AddText(ImVec2(bx,by-14), IM_COL32(255,255,255,200), e.name);

            // Distance text
            char dst[32]; sprintf_s(dst, "%.0fm", e.dist);
            dl->AddText(ImVec2(bx+w+4,by+h-14), IM_COL32(255,255,255,150), dst);
        }
    }

    // ====== MENU ======
    void DrawMenu(){
        ImGui::SetNextWindowSize(ImVec2(350, 420), ImGuiCond_FirstUseEver);
        ImGui::Begin("WT Research Tool", &cfg.menu, ImGuiWindowFlags_NoCollapse);

        if(ImGui::BeginTabBar("Tabs")){

            // === ESP TAB ===
            if(ImGui::BeginTabItem("ESP")){
                ImGui::Checkbox("Enable ESP", &cfg.esp);
                ImGui::Separator();
                ImGui::Checkbox("Box ESP", &cfg.espBox);
                ImGui::SameLine(); ImGui::Checkbox("Snaplines", &cfg.espSnap);
                ImGui::Checkbox("Health Bar", &cfg.espHealth);
                ImGui::SameLine(); ImGui::Checkbox("Names", &cfg.espName);
                ImGui::Checkbox("Team Check", &cfg.espTeamCheck);
                ImGui::SliderFloat("Max Distance", &cfg.espMaxDist, 100, 10000, "%.0fm");
                ImGui::EndTabItem();
            }

            // === AIMBOT TAB ===
            if(ImGui::BeginTabItem("Aimbot")){
                ImGui::Checkbox("Enable Aimbot", &cfg.aim);
                ImGui::Separator();
                ImGui::SliderFloat("FOV", &cfg.aimFov, 1.0f, 30.0f, "%.1f px");
                ImGui::SliderFloat("Smoothness", &cfg.aimSmooth, 1.0f, 20.0f, "%.1f");
                ImGui::Combo("Activation Key", &cfg.aimKey, aimKeys, IM_ARRAYSIZE(aimKeys));
                ImGui::EndTabItem();
            }

            // === TRIGGERBOT TAB ===
            if(ImGui::BeginTabItem("Triggerbot")){
                ImGui::Checkbox("Enable Triggerbot", &cfg.trig);
                ImGui::Separator();
                ImGui::SliderFloat("FOV", &cfg.trigFov, 1.0f, 50.0f, "%.1f px");
                ImGui::SliderInt("Delay Min", &cfg.trigMin, 0, 200, "%d ms");
                ImGui::SliderInt("Delay Max", &cfg.trigMax, 0, 200, "%d ms");
                ImGui::EndTabItem();
            }

            // === WALLBANG TAB ===
            if(ImGui::BeginTabItem("Wallbang")){
                bool wbState = cfg.wb;
                if(ImGui::Checkbox("Shoot Through Everything", &wbState)){
                    cfg.wb=wbState;
                    if(cfg.wb)gMem.EnableWallbang(); else gMem.DisableWallbang();
                }
                ImGui::Separator();
                ImGui::SliderFloat("Pen Multiplier", &cfg.penMult, 1.0f, 50.0f, "%.1fx");
                ImGui::TextWrapped("Patches the collision function so bullets pass through all geometry: walls, terrain, buildings, trees, ground.");
                if(gMem.colAddr!=0){
                    ImGui::Text("Collision func found: 0x%llX", gMem.colAddr);
                } else {
                    ImGui::TextColored(ImVec4(1,0,0,1), "Collision func not found. Wallbang may not work.");
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        ImGui::Text("INSERT - Toggle Menu | END - Unload");

        ImGui::End();
    }

    bool HookD3D(){
        // Find game window first
        HWND hGameWnd = NULL;
        for(int i=0; i<100; i++){
            hGameWnd = FindWindowW(L"SDL_app", NULL);
            if(!hGameWnd) hGameWnd = FindWindowW(NULL, L"War Thunder");
            if(!hGameWnd) EnumWindows([](HWND h, LPARAM lp)->BOOL{
                wchar_t cls[64]; GetClassNameW(h,cls,64);
                if(wcsstr(cls,L"SDL")||wcsstr(cls,L"d3d")||wcsstr(cls,L"Win32")){
                    DWORD pid; GetWindowThreadProcessId(h,&pid);
                    if(pid==GetCurrentProcessId()){*(HWND*)lp=h;return FALSE;}
                } return TRUE;
            }, (LPARAM)&hGameWnd);
            if(hGameWnd) break;
            Sleep(50);
        }
        printf("[+] Game window: 0x%llX\n", (uintptr_t)hGameWnd);

        // Create temp BUTTON class window (always registered)
        HWND hTemp = CreateWindowExA(0, "BUTTON", "", WS_POPUP, 0, 0, 1, 1, NULL, NULL, NULL, NULL);

        ID3D11Device* td = NULL;
        IDXGISwapChain* tc = NULL;

        // Try game window first, then temp, then desktop
        HWND targets[] = { hGameWnd, hTemp, GetDesktopWindow() };
        D3D_DRIVER_TYPE types[] = { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP,
                                     D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_SOFTWARE };

        for(int ti=0; ti<3; ti++){
            if(!targets[ti]) continue;
            for(int di=0; di<4; di++){
                DXGI_SWAP_CHAIN_DESC sd={};
                sd.BufferCount=1;
                sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
                sd.BufferDesc.Width=1; sd.BufferDesc.Height=1;
                sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
                sd.OutputWindow=targets[ti];
                sd.SampleDesc.Count=1;
                sd.Windowed=TRUE;

                td=NULL; tc=NULL;
                HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, types[di], NULL, 0,
                    NULL, 0, D3D11_SDK_VERSION, &sd, &tc, &td, NULL, NULL);
                if(SUCCEEDED(hr) && td && tc){
                    printf("[+] Device OK (hwnd:%p, driver:%d)\n", targets[ti], types[di]);
                    goto HOOK_IT;
                }
            }
        }

        if(hTemp) DestroyWindow(hTemp);
        printf("[!] Cannot create D3D11 device\n");
        return false;

HOOK_IT:
        if(hTemp) DestroyWindow(hTemp);
        // Keep tc alive for vtable access

        void** vt = *(void***)tc;
        printf("[*] VTable at %p\n", vt);
        for(int vi=0; vi<12; vi++) printf("  vt[%d] = %p\n", vi, vt[vi]);

        orig = (P_t)vt[8];
        DWORD old;
        VirtualProtect(&vt[8], 8, PAGE_READWRITE, &old);
        vt[8] = (void*)&D3D11Hook::Hook;
        VirtualProtect(&vt[8], 8, old, &old);

        printf("[+] VTable hooked: vt[8] = %p -> %p\n", orig, vt[8]);

        td->Release();
        tc->Release();
        return true;
    }
};
D3D11Hook* D3D11Hook::inst=nullptr;

// ===================================================================
// AIMBOT + TRIGGERBOT THREAD
// ===================================================================
DWORD WINAPI GameThread(HMODULE m){
    while(true){
        Sleep(5);

        if(cfg.aim){
            int key = (cfg.aimKey>=0&&cfg.aimKey<5) ? aimKeyCodes[cfg.aimKey] : VK_RBUTTON;
            if(GetAsyncKeyState(key)&0x8000){
                auto lp=gMem.GetLP();if(!lp.addr||!lp.alive)continue;
                auto es=gMem.GetEnts();Entity*t=nullptr;float bf=cfg.aimFov;
                int sw=1920,sh=1080;
                for(auto&e:es){
                    if(e.addr==lp.addr||!e.alive)continue;
                    if(cfg.espTeamCheck&&e.team==lp.team)continue;
                    Vector2 s;if(!gMem.W2S(e.pos,s,sw,sh))continue;
                    float d=Vector2(s.x-sw/2,s.y-sh/2).Dist(Vector2(0,0));
                    if(d<bf){bf=d;t=&e;}
                }
                if(t){
                    Vector2 a=gMem.CalcAng(lp.pos,t->pos);
                    static Vector2 cur={0,0};
                    cur.x+=(a.x-cur.x)/cfg.aimSmooth;
                    cur.y+=(a.y-cur.y)/cfg.aimSmooth;
                    gMem.SetAng(cur);
                }
            }
        }

        if(cfg.trig){
            auto lp=gMem.GetLP();if(!lp.addr||!lp.alive)continue;
            auto es=gMem.GetEnts();
            int sw=1920,sh=1080;
            for(auto&e:es){
                if(e.addr==lp.addr||!e.alive)continue;
                if(cfg.espTeamCheck&&e.team==lp.team)continue;
                Vector2 s;if(!gMem.W2S(e.pos,s,sw,sh))continue;
                float d=Vector2(s.x-sw/2,s.y-sh/2).Dist(Vector2(0,0));
                if(d<cfg.trigFov){
                    Vector2 a=gMem.CalcAng(lp.pos,e.pos);
                    gMem.SetAng(a);
                    INPUT ip={};ip.type=INPUT_MOUSE;ip.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
                    SendInput(1,&ip,sizeof(INPUT));
                    Sleep(cfg.trigMin+rand()%(max(1,cfg.trigMax-cfg.trigMin+1)));
                    ip.mi.dwFlags=MOUSEEVENTF_LEFTUP;
                    SendInput(1,&ip,sizeof(INPUT));
                    break;
                }
            }
        }

        if(GetAsyncKeyState(VK_END)&1){
            if(cfg.wb)gMem.DisableWallbang();
            // Cleanup ImGui
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            if(oWndProc && g_hWindow) SetWindowLongPtrW(g_hWindow, GWLP_WNDPROC, (LONG_PTR)oWndProc);
            ImGui::DestroyContext();
            FreeConsole();FreeLibraryAndExitThread(m,0);return 0;
        }
    }
}

// ===================================================================
// MAIN
// ===================================================================
DWORD WINAPI DelayedHookThread(HMODULE m){
    printf("[*] Waiting 5 seconds for game to initialize DirectX...\n");
    Sleep(5000);
    
    printf("[*] Attempting delayed D3D hook...\n");
    D3D11Hook d;D3D11Hook::inst=&d;
    if(d.HookD3D()){
        printf("[+] D3D11 + ImGui ready (delayed hook)\n");
    } else {
        printf("[!] Delayed hook failed - running without overlay\n");
    }
    return 0;
}

DWORD WINAPI Main(HMODULE m){
    AllocConsole();FILE*f;freopen_s(&f,"CONOUT$","w",stdout);
    printf("=== WT Research Tool v3.0 (ImGui) ===\n");
    printf("[*] Safe mode (no D3D hook): %s\n", cfg.enableHook ? "OFF" : "ON");
    
    if(!gMem.ScanAll()) printf("[!] Some offsets not found\n");

    if(cfg.enableHook){
        // Delay D3D hook to avoid early crash
        CreateThread(0,0,(LPTHREAD_START_ROUTINE)DelayedHookThread,m,0,0);
    } else {
        printf("[*] D3D hook disabled - running in safe mode\n");
    }

    CreateThread(0,0,(LPTHREAD_START_ROUTINE)GameThread,m,0,0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE m,DWORD r,LPVOID){
    if(r==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(m);CreateThread(0,0,(LPTHREAD_START_ROUTINE)Main,m,0,0);}
    return TRUE;
}
