#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <dwrite.h>
#include <D3Dcompiler.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <TlHelp32.h>
#include <random>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "D3Dcompiler.lib")

using namespace DirectX;

// ===================================================================
// MATH TYPES
// ===================================================================
struct Vector2 { float x,y;
    Vector2():x(0),y(0){} Vector2(float a,float b):x(a),y(b){}
    Vector2 operator+(Vector2 o){return{x+o.x,y+o.y};}
    Vector2 operator-(Vector2 o){return{x-o.x,y-o.y};}
    float Dot(Vector2 o){return x*o.x+y*o.y;}
    float Len(){return sqrtf(x*x+y*y);}
    float Dist(Vector2 o){Vector2 d=*this-o;return d.Len();}
};
struct Vector3 { float x,y,z;
    Vector3():x(0),y(0),z(0){} Vector3(float a,float b,float c):x(a),y(b),z(c){}
    Vector3 operator+(Vector3 o){return{x+o.x,y+o.y,z+o.z};}
    Vector3 operator-(Vector3 o){return{x-o.x,y-o.y,z-o.z};}
    Vector3 operator*(float s){return{x*s,y*s,z*s};}
    Vector3 operator/(float s){return{x/s,y/s,z/s};}
    float Len(){return sqrtf(x*x+y*y+z*z);}
    Vector3 Norm(){float l=Len();return l>0?*this/l:Vector3(0,0,0);}
};
struct Color { float r,g,b,a;
    Color():r(1),g(1),b(1),a(1){} Color(float R,float G,float B,float A):r(R),g(G),b(B),a(A){}
    static Color Red(){return{1,0,0,1};} static Color Green(){return{0,1,0,1};}
    static Color Blue(){return{0,0,1,1};} static Color White(){return{1,1,1,1};}
    static Color Black(){return{0,0,0,1};} static Color Yellow(){return{1,1,0,1};}
    static Color Cyan(){return{0,1,1,1};} static Color Orange(){return{1,0.5f,0,1};}
    Color Alpha(float a){return{r,g,b,a};}
};

// ===================================================================
// ENTITY & CONFIG
// ===================================================================
struct Entity {
    uintptr_t addr=0;
    Vector3 pos,head;
    float hp=0,mhp=100;
    int team=0;
    char name[64]={};
    bool alive=false;
    float dist=0;
    Vector2 sp,sh,boxTL,boxBR;
};

struct Config {
    bool esp=true,espBox=true,espSnap=true,espHealth=true,espDist=true,espName=true;
    bool espTeamCheck=true; float espMaxDist=5000.0f;
    bool aim=true; float aimFov=6.0f,aimSmooth=4.0f; int aimKey=VK_RBUTTON;
    bool trig=false; float trigFov=12.0f; int trigMin=15,trigMax=35;
    bool wb=false,wbPenBoost=false; float penMult=20.0f;
    bool menu=true; int mx=10,my=10;
}cfg;

// ===================================================================
// MEMORY MANAGER
// ===================================================================
class Memory {
    HANDLE p; uintptr_t base=0,gPtr=0,lpPtr=0,plPtr=0,vmAddr=0,colAddr=0;
    uint32_t oPos=0x80,oHead=0x90,oHp=0xC0,oMhp=0xC4,oTeam=0xD0,oName=0x100,oAlive=0x1A0,oAng=0x1A0;
    uint8_t colOrig[32]; int colSize=0; bool colSaved=false;

    uintptr_t RP(uintptr_t a){return R<uintptr_t>(a);}

public:
    Memory():p(GetCurrentProcess()){}

    bool HasColAddr(){return colAddr!=0;}

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

        uintptr_t a;
        a=FindSig(p1,"xxx????x");if(a){gPtr=RIP(a);printf("[+] cGame: 0x%llX\n",gPtr);}
        a=FindSig(p2,"xxx????xxxx");if(a){lpPtr=RIP(a);printf("[+] cLocalPlayer: 0x%llX\n",lpPtr);}
        a=FindSig(p3,"xxx????xxxx");if(a){plPtr=RIP(a);printf("[+] cPlayerList: 0x%llX\n",plPtr);}
        a=FindSig(p4,"xxx????xxx");if(a){vmAddr=RIP(a);printf("[+] ViewMatrix: 0x%llX\n",vmAddr);}

        // Try wallbang signatures
        uint8_t pCol1[]={0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9};
        a=FindSig(pCol1,"xxxxxxxxx");if(a){colAddr=a;colSize=5;printf("[+] Collision func: 0x%llX\n",colAddr);}
        if(!colAddr){
            uint8_t pCol2[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20};
            a=FindSig(pCol2,"xxxxxxxxxxxxxxx");if(a){colAddr=a;colSize=5;printf("[+] Collision func (alt): 0x%llX\n",colAddr);}
        }

        // Auto-detect entity fields
        if(plPtr){
            uintptr_t g=RP(base+gPtr);if(g){
            uintptr_t l=RP(g+plPtr);if(l){
            uintptr_t e=RP(l);if(e){
                printf("[*] Probing entity at 0x%llX\n",e);
                uint8_t b[0x400];RB(e,b,0x400);
                for(int i=0;i<0x200;i+=4)if(*(float*)&b[i]==100.0f){oHp=i;printf("[+] hpOff:0x%X\n",i);break;}
                for(int i=0;i<0x200;i+=4){int v=*(int*)&b[i];if(v>=0&&v<=5){oTeam=i;printf("[+] teamOff:0x%X(val:%d)\n",i,v);break;}}
                for(int i=0;i<0x200;i+=4){
                    float f1=*(float*)&b[i],f2=*(float*)&b[i+4],f3=*(float*)&b[i+8];
                    if(fabs(f1)>50&&fabs(f2)>50&&fabs(f3)>0&&fabs(f1)<100000){oPos=i;oHead=i;printf("[+] posOff:0x%X\n",i);break;}
                }
                for(int i=0;i<0x200;i++)if(b[i]>='A'&&b[i]<='Z'&&b[i+1]>='a'){oName=i;char t[32]={};memcpy(t,&b[i],31);printf("[+] nameOff:0x%X(%s)\n",i,t);break;}
            }}}
        }
        printf("[*] Entity offsets - pos:0x%X hp:0x%X team:0x%X name:0x%X\n",oPos,oHp,oTeam,oName);
        return gPtr!=0&&lpPtr!=0&&plPtr!=0&&vmAddr!=0;
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
        uintptr_t l=RP(g+plPtr);if(!l)return v;int c=R<int>(l+0x8);
        if(c>64||c<0)c=50;
        for(int i=0;i<c;i++){
            uintptr_t a=RP(l+0x10+i*8);if(!a)continue;
            Entity e;e.addr=a;e.pos=R<Vector3>(a+oPos);e.head=R<Vector3>(a+oHead);
            e.hp=R<float>(a+oHp);e.mhp=R<float>(a+oMhp);e.team=R<int>(a+oTeam);
            RB(a+oName,e.name,64);e.alive=e.hp>0.1f;v.push_back(e);
        }
        return v;
    }

    XMMATRIX GetVM(){XMMATRIX m;RB(vmAddr,&m,sizeof(m));return m;}

    bool W2S(Vector3 w,Vector2&s,int sw=1920,int sh=1080){
        XMMATRIX vm=GetVM();XMVECTOR v=XMLoadFloat3((XMFLOAT3*)&w);
        XMVECTOR t=XMVector3Transform(v,vm);float wv=XMVectorGetW(t);
        if(wv<0.01f)return false;
        s.x=(sw/2.0f)*(1+XMVectorGetX(t)/wv);s.y=(sh/2.0f)*(1-XMVectorGetY(t)/wv);return true;
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
        if(!colAddr){
            uint8_t pCol1[]={0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9};
            uintptr_t a=FindSig(pCol1,"xxxxxxxxx");if(a){colAddr=a;colSize=5;}
            if(!colAddr){
                uint8_t pCol2[]={0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x20};
                a=FindSig(pCol2,"xxxxxxxxxxxxxxx");if(a){colAddr=a;colSize=5;}
            }
            if(!colAddr){printf("[!] Cannot find collision func for wallbang\n");return;}
            printf("[+] Found collision func at 0x%llX\n",colAddr);
        }
        if(!colSaved){RB(colAddr,colOrig,colSize);colSaved=true;}
        uint8_t nops[32];memset(nops,0x90,colSize);
        WB(colAddr,nops,colSize);
        printf("[+] Wallbang ON (patched 0x%llX)\n",colAddr);
    }

    void DisableWallbang(){
        if(!colAddr||!colSaved)return;
        WB(colAddr,colOrig,colSize);
        printf("[-] Wallbang OFF\n");
    }
}gMem;

// ===================================================================
// D3D11 RENDERER
// ===================================================================
class D3D11 {
    ID3D11Device*dev=nullptr;ID3D11DeviceContext*ctx=nullptr;
    IDXGISwapChain*sc=nullptr;ID3D11RenderTargetView*rtv=nullptr;
    ID3D11BlendState*bl=nullptr;ID3D11VertexShader*vs=nullptr;
    ID3D11PixelShader*ps=nullptr;ID3D11InputLayout*il=nullptr;
    ID3D11Buffer*vb=nullptr;ID3D11Buffer*cb=nullptr;
    int sw=1920,sh=1080;bool init=false;
    typedef HRESULT(__stdcall*P_t)(IDXGISwapChain*,UINT,UINT);P_t orig=nullptr;
    struct Vert{float x,y,z,w;float r,g,b,a;};
    struct Mat4x4{float m[4][4];};

    const char*vsSrc="cbuffer CB{float4x4 p;};struct VSIn{float4 p:POSITION;float4 c:COLOR;};struct PSIn{float4 p:SV_POSITION;float4 c:COLOR;};PSIn main(VSIn i){PSIn o;o.p=mul(p,i.p);o.c=i.c;return o;}";
    const char*psSrc="struct PSIn{float4 p:SV_POSITION;float4 c:COLOR;};float4 main(PSIn i):SV_TARGET{return i.c;}";

public:
    static D3D11*inst;
    static HRESULT __stdcall Hook(IDXGISwapChain*c,UINT s,UINT f){return inst->OnPresent(c,s,f);}

    HRESULT OnPresent(IDXGISwapChain*c,UINT s,UINT f){
        if(!init){
            sc=c;sc->GetDevice(__uuidof(ID3D11Device),(void**)&dev);dev->GetImmediateContext(&ctx);
            ID3D11Texture2D*bb=nullptr;sc->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&bb);
            dev->CreateRenderTargetView(bb,nullptr,&rtv);bb->Release();
            DXGI_SWAP_CHAIN_DESC d;sc->GetDesc(&d);sw=d.BufferDesc.Width;sh=d.BufferDesc.Height;

            D3D11_BLEND_DESC bd={};
            bd.RenderTarget[0].BlendEnable=TRUE;
            bd.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;
            bd.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
            bd.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
            bd.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
            bd.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;
            bd.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
            bd.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
            dev->CreateBlendState(&bd,&bl);

            ID3DBlob*eb=nullptr,*vbB=nullptr,*pbB=nullptr;
            D3DCompile(vsSrc,strlen(vsSrc),nullptr,nullptr,nullptr,"main","vs_4_0",0,0,&vbB,&eb);
            if(vbB){dev->CreateVertexShader(vbB->GetBufferPointer(),vbB->GetBufferSize(),nullptr,&vs);}
            D3DCompile(psSrc,strlen(psSrc),nullptr,nullptr,nullptr,"main","ps_4_0",0,0,&pbB,&eb);
            if(pbB){dev->CreatePixelShader(pbB->GetBufferPointer(),pbB->GetBufferSize(),nullptr,&ps);}

            D3D11_INPUT_ELEMENT_DESC ied[]={
                {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
                {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}
            };
            if(vbB){dev->CreateInputLayout(ied,2,vbB->GetBufferPointer(),vbB->GetBufferSize(),&il);vbB->Release();}
            if(pbB)pbB->Release();

            D3D11_BUFFER_DESC bbd={};bbd.ByteWidth=sizeof(Vert)*6;bbd.Usage=D3D11_USAGE_DYNAMIC;
            bbd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bbd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            dev->CreateBuffer(&bbd,nullptr,&vb);

            Mat4x4 ortho={};ortho.m[0][0]=2.0f/sw;ortho.m[1][1]=-2.0f/sh;ortho.m[2][2]=1;ortho.m[3][3]=1;
            ortho.m[3][0]=-1;ortho.m[3][1]=1;
            D3D11_BUFFER_DESC cbd={};cbd.ByteWidth=sizeof(Mat4x4);cbd.Usage=D3D11_USAGE_DYNAMIC;
            cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;cbd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            dev->CreateBuffer(&cbd,nullptr,&cb);
            D3D11_MAPPED_SUBRESOURCE ms;ctx->Map(cb,0,D3D11_MAP_WRITE_DISCARD,0,&ms);
            memcpy(ms.pData,&ortho,sizeof(ortho));ctx->Unmap(cb,0);

            init=true;printf("[+] D3D11 init (%dx%d)\n",sw,sh);
        }

        float bf[4]={1,1,1,1};ctx->OMSetBlendState(bl,bf,0xFFFFFFFF);
        ctx->OMSetRenderTargets(1,&rtv,nullptr);
        UINT stride=sizeof(Vert),off=0;
        ctx->IASetVertexBuffers(0,1,&vb,&stride,&off);
        ctx->IASetInputLayout(il);
        ctx->VSSetShader(vs,nullptr,0);ctx->VSSetConstantBuffers(0,1,&cb);
        ctx->PSSetShader(ps,nullptr,0);

        DoAll();
        return orig(c,s,f);
    }

    void DrawLine(float x1,float y1,float x2,float y2,Color c){
        Vert v[2]={{x1,y1,0,1,c.r,c.g,c.b,c.a},{x2,y2,0,1,c.r,c.g,c.b,c.a}};
        D3D11_MAPPED_SUBRESOURCE ms;
        if(SUCCEEDED(ctx->Map(vb,0,D3D11_MAP_WRITE_DISCARD,0,&ms))){
            memcpy(ms.pData,v,sizeof(v));ctx->Unmap(vb,0);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            ctx->Draw(2,0);
        }
    }

    void DrawRect(float x,float y,float w,float h,Color c){
        DrawLine(x,y,x+w,y,c);DrawLine(x+w,y,x+w,y+h,c);
        DrawLine(x+w,y+h,x,y+h,c);DrawLine(x,y+h,x,y,c);
    }

    void DrawFill(float x,float y,float w,float h,Color c){
        Vert v[6]={{x,y,0,1,c.r,c.g,c.b,c.a},{x+w,y,0,1,c.r,c.g,c.b,c.a},{x,y+h,0,1,c.r,c.g,c.b,c.a},
                   {x+w,y,0,1,c.r,c.g,c.b,c.a},{x+w,y+h,0,1,c.r,c.g,c.b,c.a},{x,y+h,0,1,c.r,c.g,c.b,c.a}};
        D3D11_MAPPED_SUBRESOURCE ms;
        if(SUCCEEDED(ctx->Map(vb,0,D3D11_MAP_WRITE_DISCARD,0,&ms))){
            memcpy(ms.pData,v,sizeof(v));ctx->Unmap(vb,0);
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->Draw(6,0);
        }
    }

    void DoESP(){
        auto lp=gMem.GetLP();if(!lp.addr)return;
        auto es=gMem.GetEnts();
        for(auto&e:es){
            if(e.addr==lp.addr||!e.alive)continue;
            if(cfg.espTeamCheck&&e.team==lp.team)continue;
            e.dist=(e.pos-lp.pos).Len();
            if(e.dist>cfg.espMaxDist)continue;
            if(!gMem.W2S(e.pos,e.sp,sw,sh)||!gMem.W2S(e.head,e.sh,sw,sh))continue;

            float h=fabs(e.sp.y-e.sh.y);if(h<5)h=e.dist*0.05f;
            float w=h*0.6f;if(w<6)w=6;if(h<6)h=6;
            float bx=e.sp.x-w/2,by=e.sh.y;

            Color ec=(e.team==lp.team)?Color::Green():Color::Red();
            float alpha=(e.dist>1000)?max(0.2f,1-e.dist/5000):1.0f;
            ec=ec.Alpha(alpha);

            if(cfg.espBox)DrawRect(bx,by,w,h,ec);
            if(cfg.espSnap)DrawLine(sw/2,sh,e.sp.x,e.sp.y,Color::White().Alpha(0.3f));
            if(cfg.espHealth){
                float p=e.hp/e.mhp;if(p<0)p=0;
                DrawFill(bx-6,by,3,h,Color::Black().Alpha(0.6f));
                Color hc=(p>0.5f)?Color::Green():((p>0.25f)?Color::Yellow():Color::Red());
                DrawFill(bx-6,by+h*(1-p),3,h*p,hc);
            }
        }
    }

    void DoAim(){
        if(!cfg.aim||!(GetAsyncKeyState(cfg.aimKey)&0x8000))return;
        auto lp=gMem.GetLP();if(!lp.addr||!lp.alive)return;
        auto es=gMem.GetEnts();Entity*t=nullptr;float bf=cfg.aimFov;
        for(auto&e:es){
            if(e.addr==lp.addr||!e.alive)continue;
            if(cfg.espTeamCheck&&e.team==lp.team)continue;
            Vector2 s;if(!gMem.W2S(e.pos,s,sw,sh))continue;
            float d=Vector2(s.x-sw/2,s.y-sh/2).Len();
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

    void DoTrig(){
        if(!cfg.trig)return;
        auto lp=gMem.GetLP();if(!lp.addr||!lp.alive)return;
        auto es=gMem.GetEnts();
        for(auto&e:es){
            if(e.addr==lp.addr||!e.alive)continue;
            if(cfg.espTeamCheck&&e.team==lp.team)continue;
            Vector2 s;if(!gMem.W2S(e.pos,s,sw,sh))continue;
            float d=Vector2(s.x-sw/2,s.y-sh/2).Len();
            if(d<cfg.trigFov){
                // Set aim on target and fire
                Vector2 a=gMem.CalcAng(lp.pos,e.pos);
                gMem.SetAng(a);
                INPUT ip={};ip.type=INPUT_MOUSE;ip.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
                SendInput(1,&ip,sizeof(INPUT));
                Sleep(cfg.trigMin+rand()%(cfg.trigMax-cfg.trigMin+1));
                ip.mi.dwFlags=MOUSEEVENTF_LEFTUP;
                SendInput(1,&ip,sizeof(INPUT));
                break;
            }
        }
    }

    void DoMenu(){
        if(!cfg.menu)return;
        int x=cfg.mx,y=cfg.my,w=200,h=200;
        DrawFill(x,y,w,h,Color::Black().Alpha(0.8f));
        DrawRect(x,y,w,h,Color::Cyan());

        int ly=y+15;
        // ESP
        DrawFill(x+5,ly-1,6,6,cfg.esp?Color::Green():Color::Red()); ly+=12;
        DrawFill(x+10,ly-1,5,5,cfg.espBox?Color::Green():Color::Red().Alpha(0.5f));
        DrawFill(x+30,ly-1,5,5,cfg.espSnap?Color::Green():Color::Red().Alpha(0.5f));
        DrawFill(x+50,ly-1,5,5,cfg.espHealth?Color::Green():Color::Red().Alpha(0.5f));
        DrawFill(x+70,ly-1,5,5,cfg.espTeamCheck?Color::Green():Color::Red().Alpha(0.5f));
        ly+=15;
        // Aim
        DrawFill(x+5,ly-1,6,6,cfg.aim?Color::Green():Color::Red()); ly+=12;
        // Trig
        DrawFill(x+5,ly-1,6,6,cfg.trig?Color::Green():Color::Red()); ly+=12;
        // Wallbang
        DrawFill(x+5,ly-1,6,6,cfg.wb?Color::Green():Color::Red());
    }

    void DoAll(){
        if(cfg.esp)DoESP();
        DoAim();
        DoTrig();
        if(cfg.menu)DoMenu();
    }

    bool HookD3D(){
        ID3D11Device*td=nullptr;IDXGISwapChain*tc=nullptr;
        DXGI_SWAP_CHAIN_DESC sd={};sd.BufferCount=1;
        sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width=1;sd.BufferDesc.Height=1;
        sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow=GetDesktopWindow();sd.SampleDesc.Count=1;sd.Windowed=TRUE;
        if(FAILED(D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&sd,&tc,&td,NULL,NULL)))
            if(FAILED(D3D11CreateDeviceAndSwapChain(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,NULL,0,D3D11_SDK_VERSION,&sd,&tc,&td,NULL,NULL)))
                return false;
        void**vt=*(void***)tc;orig=(P_t)vt[8];
        DWORD old;VirtualProtect(&vt[8],8,PAGE_READWRITE,&old);
        vt[8]=Hook;VirtualProtect(&vt[8],8,old,&old);
        td->Release();tc->Release();
        return true;
    }
};
D3D11* D3D11::inst=nullptr;

// ===================================================================
// HOTKEY THREAD
// ===================================================================
DWORD WINAPI Hotkeys(HMODULE m){
    while(true){
        Sleep(50);
        if(GetAsyncKeyState(VK_INSERT)&1)cfg.menu=!cfg.menu;
        if(GetAsyncKeyState(VK_HOME)&1){cfg.aim=!cfg.aim;printf("[%s] Aimbot\n",cfg.aim?"ON":"OFF");}
        if(GetAsyncKeyState(VK_DELETE)&1){cfg.trig=!cfg.trig;printf("[%s] Triggerbot\n",cfg.trig?"ON":"OFF");}
        if(GetAsyncKeyState(VK_PRIOR)&1){
            cfg.wb=!cfg.wb;
            if(cfg.wb)gMem.EnableWallbang(); else gMem.DisableWallbang();
        }
        if(GetAsyncKeyState(VK_F1)&1)cfg.esp=!cfg.esp;
        if(GetAsyncKeyState(VK_F2)&1)cfg.espBox=!cfg.espBox;
        if(GetAsyncKeyState(VK_F3)&1)cfg.espSnap=!cfg.espSnap;
        if(GetAsyncKeyState(VK_F4)&1)cfg.espHealth=!cfg.espHealth;
        if(GetAsyncKeyState(VK_F5)&1)cfg.espTeamCheck=!cfg.espTeamCheck;
        if(GetAsyncKeyState('1')&1){cfg.aimFov=max(1.0f,cfg.aimFov-1);printf("FOV: %.1f\n",cfg.aimFov);}
        if(GetAsyncKeyState('2')&1){cfg.aimFov=min(30.0f,cfg.aimFov+1);printf("FOV: %.1f\n",cfg.aimFov);}
        if(GetAsyncKeyState('3')&1){cfg.aimSmooth=max(1.0f,cfg.aimSmooth-1);printf("Smooth: %.1f\n",cfg.aimSmooth);}
        if(GetAsyncKeyState('4')&1){cfg.aimSmooth=min(20.0f,cfg.aimSmooth+1);printf("Smooth: %.1f\n",cfg.aimSmooth);}
        if(GetAsyncKeyState(VK_END)&1){
            printf("[*] Shutting down...\n");
            if(cfg.wb)gMem.DisableWallbang();
            FreeConsole();FreeLibraryAndExitThread(m,0);return 0;
        }
    }
}

// ===================================================================
// MAIN
// ===================================================================
DWORD WINAPI Main(HMODULE m){
    AllocConsole();FILE*f;freopen_s(&f,"CONOUT$","w",stdout);
    printf("=== WT Research Tool v2.0 ===\n");
    printf("[*] Scanning offsets...\n");

    if(!gMem.ScanAll()){
        printf("[!] Some offsets not found\n");
    }

    D3D11 d;D3D11::inst=&d;
    if(d.HookD3D())printf("[+] D3D11 hooked\n");
    else printf("[!] D3D11 hook failed\n");

    printf("\n=== HOTKEYS ===\n");
    printf("INSERT  - Menu\nHOME    - Aimbot\nDELETE  - Triggerbot\n");
    printf("PGUP    - Wallbang\nF1      - ESP\nF2-F5   - ESP features\n");
    printf("1/2     - FOV -/+\n3/4     - Smooth -/+\nEND     - Unload\n");

    CreateThread(0,0,(LPTHREAD_START_ROUTINE)Hotkeys,m,0,0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE m,DWORD r,LPVOID){
    if(r==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(m);CreateThread(0,0,(LPTHREAD_START_ROUTINE)Main,m,0,0);}
    return TRUE;
}
