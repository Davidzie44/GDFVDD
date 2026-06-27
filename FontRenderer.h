#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <map>
#include <vector>
#include "Common.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "gdi32.lib")

struct CharInfo {
    float x;      // X position in texture
    float y;      // Y position in texture
    float width;  // Character width
    float height; // Character height
    float xAdvance; // Advance to next character
};

class CDX11Font {
private:
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    ID3D11Texture2D* fontTexture;
    ID3D11ShaderResourceView* fontSRV;
    ID3D11SamplerState* sampler;
    ID3D11BlendState* blendState;
    
    std::wstring fontName;
    int fontSize;
    int fontWeight;
    
    int textureWidth;
    int textureHeight;
    
    CharInfo charInfo[256]; // ASCII 0-255
    
    bool CreateFontTexture() {
        // Create a large texture atlas using GDI
        textureWidth = 512;
        textureHeight = 512;
        
        // Create GDI bitmap
        HDC hdc = CreateCompatibleDC(nullptr);
        if (!hdc) return false;
        
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = textureWidth;
        bmi.bmiHeader.biHeight = -textureHeight; // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        void* bitmapBits = nullptr;
        HBITMAP hbitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bitmapBits, nullptr, 0);
        if (!hbitmap || !bitmapBits) {
            DeleteDC(hdc);
            return false;
        }
        
        HBITMAP oldBitmap = (HBITMAP)SelectObject(hdc, hbitmap);
        
        // Create font
        LOGFONTW lf = {};
        lf.lfHeight = -fontSize;
        lf.lfWeight = fontWeight;
        lf.lfQuality = ANTIALIASED_QUALITY;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        wcscpy_s(lf.lfFaceName, fontName.c_str());
        
        HFONT hfont = CreateFontIndirectW(&lf);
        HFONT oldFont = (HFONT)SelectObject(hdc, hfont);
        
        // Clear background to transparent (black with alpha 0)
        memset(bitmapBits, 0, textureWidth * textureHeight * 4);
        
        // Set text color to white
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        
        // Draw characters to texture atlas
        int x = 0;
        int y = 0;
        int maxHeight = fontSize;
        
        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        
        for (int c = 32; c < 127; c++) { // Printable ASCII
            wchar_t ch = (wchar_t)c;
            
            // Get character width
            SIZE size;
            GetTextExtentPoint32W(hdc, &ch, 1, &size);
            
            // Check if we need to move to next row
            if (x + size.cx > textureWidth) {
                x = 0;
                y += maxHeight + 2;
            }
            
            // Draw character
            ExtTextOutW(hdc, x, y, ETO_OPAQUE, nullptr, &ch, 1, nullptr);
            
            // Store character info
            charInfo[c].x = (float)x;
            charInfo[c].y = (float)y;
            charInfo[c].width = (float)size.cx;
            charInfo[c].height = (float)size.cy;
            charInfo[c].xAdvance = (float)size.cx;
            
            x += size.cx + 2;
        }
        
        // Clean up GDI
        SelectObject(hdc, oldFont);
        DeleteObject(hfont);
        SelectObject(hdc, oldBitmap);
        DeleteObject(hbitmap);
        DeleteDC(hdc);
        
        // Create D3D11 texture from bitmap data
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = textureWidth;
        texDesc.Height = textureHeight;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_IMMUTABLE;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = bitmapBits;
        initData.SysMemPitch = textureWidth * 4;
        
        // Note: bitmapBits was freed when we deleted the DC, so we need to recreate it
        // For simplicity, we'll use a different approach - create texture then update it
        
        if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &fontTexture))) {
            return false;
        }
        
        // Update texture with GDI data (need to recreate the bitmap data)
        // For now, we'll use a simpler approach - create empty texture and use UpdateSubresource
        
        return true;
    }
    
    bool CreateFontTextureAlternative() {
        // Alternative: Use GDI+ or DirectWrite for better quality
        // For now, use a simpler approach with GDI
        
        textureWidth = 512;
        textureHeight = 512;
        
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = textureWidth;
        bmi.bmiHeader.biHeight = -textureHeight;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        
        DWORD* bitmapBits = nullptr;
        HBITMAP hbitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, (void**)&bitmapBits, nullptr, 0);
        
        HBITMAP oldBitmap = (HBITMAP)SelectObject(hdcMem, hbitmap);
        
        // Create font
        LOGFONTW lf = {};
        lf.lfHeight = -fontSize;
        lf.lfWeight = fontWeight;
        lf.lfQuality = ANTIALIASED_QUALITY;
        lf.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(lf.lfFaceName, fontName.c_str());
        
        HFONT hfont = CreateFontIndirectW(&lf);
        HFONT oldFont = (HFONT)SelectObject(hdcMem, hfont);
        
        // Clear to transparent
        memset(bitmapBits, 0, textureWidth * textureHeight * 4);
        
        SetTextColor(hdcMem, RGB(255, 255, 255));
        SetBkMode(hdcMem, TRANSPARENT);
        
        // Draw characters
        int x = 0;
        int y = 0;
        int rowHeight = fontSize + 4;
        
        for (int c = 32; c < 127; c++) {
            wchar_t ch = (wchar_t)c;
            SIZE size;
            GetTextExtentPoint32W(hdcMem, &ch, 1, &size);
            
            if (x + size.cx > textureWidth) {
                x = 0;
                y += rowHeight;
            }
            
            ExtTextOutW(hdcMem, x, y, 0, nullptr, &ch, 1, nullptr);
            
            charInfo[c].x = (float)x;
            charInfo[c].y = (float)y;
            charInfo[c].width = (float)size.cx;
            charInfo[c].height = (float)size.cy;
            charInfo[c].xAdvance = (float)size.cx;
            
            x += size.cx + 2;
        }
        
        SelectObject(hdcMem, oldFont);
        DeleteObject(hfont);
        SelectObject(hdcMem, oldBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        
        // Create D3D11 texture
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = textureWidth;
        texDesc.Height = textureHeight;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_IMMUTABLE;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = bitmapBits;
        initData.SysMemPitch = textureWidth * 4;
        
        HRESULT hr = device->CreateTexture2D(&texDesc, &initData, &fontTexture);
        
        DeleteObject(hbitmap);
        
        if (FAILED(hr)) return false;
        
        // Create shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        
        if (FAILED(device->CreateShaderResourceView(fontTexture, &srvDesc, &fontSRV))) {
            return false;
        }
        
        // Create sampler
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        
        if (FAILED(device->CreateSamplerState(&sampDesc, &sampler))) {
            return false;
        }
        
        // Create blend state for text
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        
        if (FAILED(device->CreateBlendState(&blendDesc, &blendState))) {
            return false;
        }
        
        return true;
    }

public:
    CDX11Font() : device(nullptr), context(nullptr), fontTexture(nullptr),
                  fontSRV(nullptr), sampler(nullptr), blendState(nullptr),
                  fontSize(14), fontWeight(FW_NORMAL), textureWidth(0), textureHeight(0) {
        fontName = L"Segoe UI";
        
        // Initialize char info to defaults
        for (int i = 0; i < 256; i++) {
            charInfo[i].x = 0;
            charInfo[i].y = 0;
            charInfo[i].width = fontSize;
            charInfo[i].height = fontSize;
            charInfo[i].xAdvance = fontSize;
        }
    }
    
    ~CDX11Font() {
        if (blendState) blendState->Release();
        if (sampler) sampler->Release();
        if (fontSRV) fontSRV->Release();
        if (fontTexture) fontTexture->Release();
    }
    
    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, 
                    const std::wstring& name, int size, int weight = FW_NORMAL) {
        device = dev;
        context = ctx;
        fontName = name;
        fontSize = size;
        fontWeight = weight;
        
        return CreateFontTextureAlternative();
    }
    
    void GetTextSize(const char* text, float& outWidth, float& outHeight) {
        outWidth = 0;
        outHeight = (float)fontSize;
        
        for (const char* c = text; *c; c++) {
            unsigned char ch = (unsigned char)*c;
            if (ch < 32 || ch >= 127) continue;
            
            outWidth += charInfo[ch].xAdvance;
        }
    }
    
    void DrawTextInternal(float x, float y, const char* text, D3DXCOLOR color) {
        // This would require a shader and vertex buffer setup
        // For now, this is a placeholder - the actual rendering would be done
        // by the renderer class which has access to the shaders
    }
    
    // Get character info (used by renderer)
    const CharInfo* GetCharInfo(unsigned char c) const {
        if (c < 32 || c >= 127) return nullptr;
        return &charInfo[c];
    }
    
    ID3D11ShaderResourceView* GetTexture() const {
        return fontSRV;
    }
    
    ID3D11SamplerState* GetSampler() const {
        return sampler;
    }
    
    ID3D11BlendState* GetBlendState() const {
        return blendState;
    }
    
    int GetFontSize() const {
        return fontSize;
    }
};

class CD3D11Renderer {
private:
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    
    CDX11Font* fontMain;      // 14px bold
    CDX11Font* fontSmall;     // 11px regular
    CDX11Font* fontBig;       // 20px bold
    
    // Vertex buffer for text rendering
    ID3D11Buffer* vertexBuffer;
    
    // Shaders for text rendering
    ID3D11VertexShader* textVertexShader;
    ID3D11PixelShader* textPixelShader;
    ID3D11InputLayout* textInputLayout;
    
    // Constant buffer for color
    ID3D11Buffer* colorBuffer;
    
    struct TextVertex {
        float x, y;
        float u, v;
        D3DXCOLOR color;
    };
    
    bool CreateShaders() {
        // Simple vertex shader for text
        const char* vsSource = R"(
            float4 main(float2 pos : POSITION, float2 tex : TEXCOORD, float4 col : COLOR) : SV_POSITION {
                return float4(pos, 0.0, 1.0);
            }
        )";
        
        // Simple pixel shader for text
        const char* psSource = R"(
            Texture2D tex : register(t0);
            SamplerState samp : register(s0);
            float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD, float4 col : COLOR) : SV_TARGET {
                float4 texColor = tex.Sample(samp, tex);
                return float4(col.rgb, texColor.a * col.a);
            }
        )";
        
        // Compile shaders (simplified - in production use proper compilation)
        // For now, we'll assume shaders are pre-compiled or use a different approach
        
        return true;
    }
    
    bool CreateVertexBuffer() {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = 4096 * sizeof(TextVertex);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        return SUCCEEDED(device->CreateBuffer(&bd, nullptr, &vertexBuffer));
    }

public:
    CD3D11Renderer() : device(nullptr), context(nullptr), fontMain(nullptr),
                       fontSmall(nullptr), fontBig(nullptr), vertexBuffer(nullptr),
                       textVertexShader(nullptr), textPixelShader(nullptr),
                       textInputLayout(nullptr), colorBuffer(nullptr) {}
    
    ~CD3D11Renderer() {
        if (colorBuffer) colorBuffer->Release();
        if (textInputLayout) textInputLayout->Release();
        if (textPixelShader) textPixelShader->Release();
        if (textVertexShader) textVertexShader->Release();
        if (vertexBuffer) vertexBuffer->Release();
        
        delete fontBig;
        delete fontSmall;
        delete fontMain;
    }
    
    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        device = dev;
        context = ctx;
        
        // Create fonts
        fontMain = new CDX11Font();
        if (!fontMain->Initialize(device, context, L"Segoe UI", 14, FW_BOLD)) {
            return false;
        }
        
        fontSmall = new CDX11Font();
        if (!fontSmall->Initialize(device, context, L"Segoe UI", 11, FW_NORMAL)) {
            return false;
        }
        
        fontBig = new CDX11Font();
        if (!fontBig->Initialize(device, context, L"Segoe UI", 20, FW_BOLD)) {
            return false;
        }
        
        // Create rendering resources
        if (!CreateShaders()) return false;
        if (!CreateVertexBuffer()) return false;
        
        return true;
    }
    
    void DrawText(float x, float y, const char* text, D3DXCOLOR color, 
                  bool centered = false, CDX11Font* font = nullptr) {
        if (!font) font = fontMain;
        
        float textWidth, textHeight;
        font->GetTextSize(text, textWidth, textHeight);
        
        if (centered) {
            x -= textWidth / 2.0f;
        }
        
        // For now, use GDI fallback for simplicity
        // In production, this would use the vertex buffer and shaders
        HDC hdc = GetDC(nullptr);
        
        LOGFONTW lf = {};
        lf.lfHeight = -font->GetFontSize();
        lf.lfWeight = (font == fontMain || font == fontBig) ? FW_BOLD : FW_NORMAL;
        lf.lfQuality = ANTIALIASED_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        
        HFONT hfont = CreateFontIndirectW(&lf);
        HFONT oldFont = (HFONT)SelectObject(hdc, hfont);
        
        SetTextColor(hdc, RGB((int)(color.r * 255), (int)(color.g * 255), (int)(color.b * 255)));
        SetBkMode(hdc, TRANSPARENT);
        
        std::wstring wtext(text, text + strlen(text));
        TextOutW(hdc, (int)x, (int)y, wtext.c_str(), (int)wtext.length());
        
        SelectObject(hdc, oldFont);
        DeleteObject(hfont);
        ReleaseDC(nullptr, hdc);
    }
    
    void DrawTextShadow(float x, float y, const char* text, D3DXCOLOR color, 
                        D3DXCOLOR shadowColor = D3DXCOLOR(0, 0, 0, 1), 
                        bool centered = false, CDX11Font* font = nullptr) {
        if (!font) font = fontMain;
        
        float textWidth, textHeight;
        font->GetTextSize(text, textWidth, textHeight);
        
        if (centered) {
            x -= textWidth / 2.0f;
        }
        
        // Draw shadow first (offset by 1,1)
        DrawText(x + 1, y + 1, text, shadowColor, false, font);
        
        // Draw main text on top
        DrawText(x, y, text, color, false, font);
    }
    
    void GetTextSize(const char* text, float& outWidth, float& outHeight, 
                     CDX11Font* font = nullptr) {
        if (!font) font = fontMain;
        font->GetTextSize(text, outWidth, outHeight);
    }
    
    // Font accessors
    CDX11Font* GetMainFont() { return fontMain; }
    CDX11Font* GetSmallFont() { return fontSmall; }
    CDX11Font* GetBigFont() { return fontBig; }
};
