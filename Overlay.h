#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <string>
#include <memory>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

struct Color {
    float r, g, b, a;
    
    Color(float red = 1.0f, float green = 1.0f, float blue = 1.0f, float alpha = 1.0f)
        : r(red), g(green), b(blue), a(alpha) {}
    
    Color(DWORD argb) {
        b = ((argb >> 16) & 0xFF) / 255.0f;
        g = ((argb >> 8) & 0xFF) / 255.0f;
        r = (argb & 0xFF) / 255.0f;
        a = ((argb >> 24) & 0xFF) / 255.0f;
    }
    
    DWORD ToARGB() const {
        return ((DWORD)(a * 255) << 24) | ((DWORD)(b * 255) << 16) | 
               ((DWORD)(g * 255) << 8) | (DWORD)(r * 255);
    }
};

struct Vertex {
    float x, y;
    Color color;
};

class D3D11Font {
private:
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    ID3D11Texture2D* fontTexture;
    ID3D11ShaderResourceView* fontSRV;
    ID3D11SamplerState* sampler;
    int charWidth;
    int charHeight;
    
public:
    D3D11Font() : device(nullptr), context(nullptr), fontTexture(nullptr), 
                  fontSRV(nullptr), sampler(nullptr), charWidth(8), charHeight(16) {}
    
    ~D3D11Font() {
        if (sampler) sampler->Release();
        if (fontSRV) fontSRV->Release();
        if (fontTexture) fontTexture->Release();
    }
    
    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        device = dev;
        context = ctx;
        
        // Create a simple font texture (8x16 per character, ASCII 32-126)
        const int texWidth = 95 * 8;
        const int texHeight = 16;
        
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = texWidth;
        texDesc.Height = texHeight;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        // Create font bitmap data (simple 8x16 font)
        std::vector<DWORD> fontData(texWidth * texHeight, 0x00000000);
        
        // Simple font patterns for basic characters (this is a minimal implementation)
        // In production, you'd load a real font file
        for (int c = 32; c < 127; c++) {
            int charIndex = c - 32;
            int charX = charIndex * 8;
            
            // Draw simple character outlines
            for (int y = 0; y < 16; y++) {
                for (int x = 0; x < 8; x++) {
                    DWORD pixel = 0xFFFFFFFF; // White
                    if (x == 0 || x == 7 || y == 0 || y == 15) {
                        pixel = 0x00000000; // Black border
                    }
                    fontData[y * texWidth + (charX + x)] = pixel;
                }
            }
        }
        
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = fontData.data();
        initData.SysMemPitch = texWidth * 4;
        
        if (FAILED(device->CreateTexture2D(&texDesc, &initData, &fontTexture))) {
            return false;
        }
        
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        
        if (FAILED(device->CreateShaderResourceView(fontTexture, &srvDesc, &fontSRV))) {
            return false;
        }
        
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        
        if (FAILED(device->CreateSamplerState(&sampDesc, &sampler))) {
            return false;
        }
        
        return true;
    }
    
    void DrawString(float x, float y, const std::string& text, Color color = Color(1,1,1,1)) {
        // This is a simplified implementation
        // In production, you'd use proper vertex batching and shaders
    }
};

class Overlay {
private:
    HWND windowHandle;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGISwapChain* swapChain;
    ID3D11RenderTargetView* renderTargetView;
    ID3D11Buffer* vertexBuffer;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11InputLayout* inputLayout;
    ID3D11BlendState* blendState;
    
    int width;
    int height;
    bool running;
    
    D3D11Font font;
    
    // Simple vertex shader
    static const char* vertexShaderSource;
    // Simple pixel shader
    static const char* pixelShaderSource;
    
    bool CreateShaders() {
        // Compile vertex shader
        ID3D10Blob* vsBlob = nullptr;
        ID3D10Blob* errorBlob = nullptr;
        
        if (FAILED(D3DCompile(vertexShaderSource, strlen(vertexShaderSource), nullptr, 
                             nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob))) {
            if (errorBlob) errorBlob->Release();
            return false;
        }
        
        if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), 
                                             nullptr, &vertexShader))) {
            vsBlob->Release();
            return false;
        }
        
        // Define input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        
        if (FAILED(device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), 
                                            vsBlob->GetBufferSize(), &inputLayout))) {
            vsBlob->Release();
            return false;
        }
        
        vsBlob->Release();
        
        // Compile pixel shader
        ID3D10Blob* psBlob = nullptr;
        
        if (FAILED(D3DCompile(pixelShaderSource, strlen(pixelShaderSource), nullptr, 
                             nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, &errorBlob))) {
            if (errorBlob) errorBlob->Release();
            return false;
        }
        
        if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), 
                                            nullptr, &pixelShader))) {
            psBlob->Release();
            return false;
        }
        
        psBlob->Release();
        return true;
    }
    
    bool CreateBlendState() {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        
        return SUCCEEDED(device->CreateBlendState(&blendDesc, &blendState));
    }
    
    bool CreateVertexBuffer() {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = 1024 * sizeof(Vertex);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        return SUCCEEDED(device->CreateBuffer(&bd, nullptr, &vertexBuffer));
    }
    
public:
    Overlay() : windowHandle(nullptr), device(nullptr), context(nullptr), 
                swapChain(nullptr), renderTargetView(nullptr), vertexBuffer(nullptr),
                vertexShader(nullptr), pixelShader(nullptr), inputLayout(nullptr),
                blendState(nullptr), width(1920), height(1080), running(false) {}
    
    ~Overlay() {
        Cleanup();
    }
    
    void Cleanup() {
        running = false;
        
        if (blendState) blendState->Release();
        if (inputLayout) inputLayout->Release();
        if (pixelShader) pixelShader->Release();
        if (vertexShader) vertexShader->Release();
        if (vertexBuffer) vertexBuffer->Release();
        if (renderTargetView) renderTargetView->Release();
        if (swapChain) swapChain->Release();
        if (context) context->Release();
        if (device) device->Release();
        if (windowHandle) DestroyWindow(windowHandle);
    }
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }
    
    bool Initialize(const std::wstring& title, int w, int h) {
        width = w;
        height = h;
        
        // Register window class
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CS2Overlay";
        
        if (!RegisterClassExW(&wc)) {
            return false;
        }
        
        // Create layered, transparent, topmost window
        windowHandle = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"CS2Overlay",
            title.c_str(),
            WS_POPUP,
            0, 0, width, height,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );
        
        if (!windowHandle) {
            return false;
        }
        
        // Set window color key (black = transparent)
        SetLayeredWindowAttributes(windowHandle, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);
        
        ShowWindow(windowHandle, SW_SHOW);
        UpdateWindow(windowHandle);
        
        // Initialize DirectX 11
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
        
        if (FAILED(hr)) {
            return false;
        }
        
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
        
        // Create shaders and resources
        if (!CreateShaders() || !CreateBlendState() || !CreateVertexBuffer()) {
            return false;
        }
        
        // Initialize font
        font.Initialize(device, context);
        
        running = true;
        return true;
    }
    
    void BeginFrame() {
        // Clear with transparent black
        float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        context->ClearRenderTargetView(renderTargetView, clearColor);
        
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(inputLayout);
        
        float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        context->OMSetBlendState(blendState, blendFactor, 0xFFFFFFFF);
        
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);
        
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    }
    
    void EndFrame() {
        swapChain->Present(1, 0); // VSync enabled for smooth rendering
    }
    
    void DrawRect(float x, float y, float w, float h, Color color) {
        Vertex vertices[6] = {
            {x, y, color},
            {x + w, y, color},
            {x, y + h, color},
            {x + w, y, color},
            {x + w, y + h, color},
            {x, y + h, color}
        };
        
        D3D11_MAPPED_SUBRESOURCE ms;
        context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, vertices, sizeof(vertices));
        context->Unmap(vertexBuffer, 0);
        
        context->Draw(6, 0);
    }
    
    void DrawFilledRect(float x, float y, float w, float h, Color color) {
        DrawRect(x, y, w, h, color);
    }
    
    void DrawLine(float x1, float y1, float x2, float y2, Color color, float thickness = 1.0f) {
        // Calculate perpendicular vector for thickness
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = std::sqrt(dx * dx + dy * dy);
        
        if (len < 0.001f) return;
        
        float nx = -dy / len * thickness * 0.5f;
        float ny = dx / len * thickness * 0.5f;
        
        Vertex vertices[6] = {
            {x1 + nx, y1 + ny, color},
            {x2 + nx, y2 + ny, color},
            {x1 - nx, y1 - ny, color},
            {x2 + nx, y2 + ny, color},
            {x2 - nx, y2 - ny, color},
            {x1 - nx, y1 - ny, color}
        };
        
        D3D11_MAPPED_SUBRESOURCE ms;
        context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, vertices, sizeof(vertices));
        context->Unmap(vertexBuffer, 0);
        
        context->Draw(6, 0);
    }
    
    void DrawCircle(float x, float y, float radius, Color color, int segments = 32) {
        std::vector<Vertex> vertices;
        
        for (int i = 0; i < segments; i++) {
            float angle1 = (float)i / segments * 3.14159f * 2.0f;
            float angle2 = (float)(i + 1) / segments * 3.14159f * 2.0f;
            
            vertices.push_back({x, y, color});
            vertices.push_back({x + std::cos(angle1) * radius, y + std::sin(angle1) * radius, color});
            vertices.push_back({x + std::cos(angle2) * radius, y + std::sin(angle2) * radius, color});
        }
        
        D3D11_MAPPED_SUBRESOURCE ms;
        context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, vertices.data(), vertices.size() * sizeof(Vertex));
        context->Unmap(vertexBuffer, 0);
        
        context->Draw(vertices.size(), 0);
    }
    
    void DrawFilledCircle(float x, float y, float radius, Color color, int segments = 32) {
        DrawCircle(x, y, radius, color, segments);
    }
    
    void DrawText(float x, float y, const std::string& text, Color color = Color(1,1,1,1), float size = 16.0f) {
        // Simplified text rendering using GDI as fallback
        HDC hdc = GetDC(windowHandle);
        
        LOGFONTW lf = {};
        lf.lfHeight = -(LONG)size;
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = ANTIALIASED_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Arial");
        
        HFONT font = CreateFontIndirectW(&lf);
        HFONT oldFont = (HFONT)SelectObject(hdc, font);
        
        SetTextColor(hdc, color.ToARGB());
        SetBkMode(hdc, TRANSPARENT);
        
        std::wstring wtext(text.begin(), text.end());
        TextOutW(hdc, (int)x, (int)y, wtext.c_str(), wtext.length());
        
        SelectObject(hdc, oldFont);
        DeleteObject(font);
        ReleaseDC(windowHandle, hdc);
    }
    
    void SetPosition(int x, int y) {
        SetWindowPos(windowHandle, HWND_TOPMOST, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    
    void Resize(int w, int h) {
        width = w;
        height = h;
        SetWindowPos(windowHandle, HWND_TOPMOST, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        
        // Recreate swap chain and render target
        context->OMSetRenderTargets(0, nullptr, nullptr);
        if (renderTargetView) renderTargetView->Release();
        
        DXGI_SWAP_CHAIN_DESC scd;
        swapChain->GetDesc(&scd);
        swapChain->ResizeBuffers(1, width, height, scd.BufferDesc.Format, 0);
        
        ID3D11Texture2D* backBuffer = nullptr;
        swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
        backBuffer->Release();
        
        context->OMSetRenderTargets(1, &renderTargetView, nullptr);
        
        D3D11_VIEWPORT vp = {};
        vp.Width = (float)width;
        vp.Height = (float)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        context->RSSetViewports(1, &vp);
    }
    
    bool IsRunning() const {
        return running;
    }
    
    HWND GetWindowHandle() const {
        return windowHandle;
    }
    
    int GetWidth() const {
        return width;
    }
    
    int GetHeight() const {
        return height;
    }
};

// Vertex shader source
const char* Overlay::vertexShaderSource = R"(
float4 main(float2 position : POSITION, float4 color : COLOR) : SV_POSITION {
    return float4(position, 0.0f, 1.0f);
}
)";

// Pixel shader source
const char* Overlay::pixelShaderSource = R"(
float4 main(float4 position : SV_POSITION, float4 color : COLOR) : SV_TARGET {
    return color;
}
)";
