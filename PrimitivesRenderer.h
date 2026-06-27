#pragma once

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <vector>
#include <cmath>
#include "Common.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct VertexPositionColor {
    float x, y;
    float r, g, b, a;
};

class CD3D11Primitives {
private:
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    
    // Vertex buffer for batching
    ID3D11Buffer* vertexBuffer;
    
    // Shaders
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11InputLayout* inputLayout;
    
    // Blend state for transparency
    ID3D11BlendState* blendState;
    
    // Batched vertices
    std::vector<VertexPositionColor> lineVertices;
    std::vector<VertexPositionColor> filledVertices;
    
    // Screen dimensions
    int screenWidth;
    int screenHeight;
    
    // Maximum vertices per batch
    static const int MAX_VERTICES = 8192;
    
    bool CreateShaders() {
        // Vertex shader for 2D orthographic rendering
        const char* vsSource = R"(
            float4 main(float2 position : POSITION, float4 color : COLOR) : SV_POSITION {
                return float4(position, 0.0, 1.0);
            }
        )";
        
        // Pixel shader (pass-through)
        const char* psSource = R"(
            float4 main(float4 position : SV_POSITION, float4 color : COLOR) : SV_TARGET {
                return color;
            }
        )";
        
        // Compile vertex shader
        ID3D10Blob* vsBlob = nullptr;
        ID3D10Blob* errorBlob = nullptr;
        
        HRESULT hr = D3DCompile(
            vsSource, strlen(vsSource), nullptr, nullptr, nullptr,
            "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob
        );
        
        if (FAILED(hr)) {
            if (errorBlob) errorBlob->Release();
            return false;
        }
        
        hr = device->CreateVertexShader(
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            nullptr, &vertexShader
        );
        
        if (FAILED(hr)) {
            vsBlob->Release();
            return false;
        }
        
        // Define input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        
        hr = device->CreateInputLayout(
            layout, 2,
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            &inputLayout
        );
        
        vsBlob->Release();
        
        if (FAILED(hr)) return false;
        
        // Compile pixel shader
        ID3D10Blob* psBlob = nullptr;
        
        hr = D3DCompile(
            psSource, strlen(psSource), nullptr, nullptr, nullptr,
            "main", "ps_4_0", 0, 0, &psBlob, &errorBlob
        );
        
        if (FAILED(hr)) {
            if (errorBlob) errorBlob->Release();
            return false;
        }
        
        hr = device->CreatePixelShader(
            psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
            nullptr, &pixelShader
        );
        
        psBlob->Release();
        
        if (FAILED(hr)) return false;
        
        return true;
    }
    
    bool CreateVertexBuffer() {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = MAX_VERTICES * sizeof(VertexPositionColor);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        return SUCCEEDED(device->CreateBuffer(&bd, nullptr, &vertexBuffer));
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
    
    void AddLineVertex(float x, float y, const D3DXCOLOR& color) {
        VertexPositionColor v;
        v.x = x;
        v.y = y;
        v.r = color.r;
        v.g = color.g;
        v.b = color.b;
        v.a = color.a;
        lineVertices.push_back(v);
    }
    
    void AddFilledVertex(float x, float y, const D3DXCOLOR& color) {
        VertexPositionColor v;
        v.x = x;
        v.y = y;
        v.r = color.r;
        v.g = color.g;
        v.b = color.b;
        v.a = color.a;
        filledVertices.push_back(v);
    }
    
    // Convert screen coordinates to clip space (-1 to 1)
    float ScreenToClipX(float x) const {
        return (x / screenWidth) * 2.0f - 1.0f;
    }
    
    float ScreenToClipY(float y) const {
        return 1.0f - (y / screenHeight) * 2.0f;
    }

public:
    CD3D11Primitives() : device(nullptr), context(nullptr), vertexBuffer(nullptr),
                       vertexShader(nullptr), pixelShader(nullptr), inputLayout(nullptr),
                       blendState(nullptr), screenWidth(1920), screenHeight(1080) {}
    
    ~CD3D11Primitives() {
        if (blendState) blendState->Release();
        if (inputLayout) inputLayout->Release();
        if (pixelShader) pixelShader->Release();
        if (vertexShader) vertexShader->Release();
        if (vertexBuffer) vertexBuffer->Release();
    }
    
    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int width = 1920, int height = 1080) {
        device = dev;
        context = ctx;
        screenWidth = width;
        screenHeight = height;
        
        if (!CreateShaders()) return false;
        if (!CreateVertexBuffer()) return false;
        if (!CreateBlendState()) return false;
        
        return true;
    }
    
    void SetScreenSize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
    }
    
    // Draw a line
    void DrawLine(float x1, float y1, float x2, float y2, 
                  const D3DXCOLOR& color, float thickness = 1.0f) {
        if (thickness <= 1.0f) {
            // Simple line
            AddLineVertex(ScreenToClipX(x1), ScreenToClipY(y1), color);
            AddLineVertex(ScreenToClipX(x2), ScreenToClipY(y2), color);
        } else {
            // Thick line (draw as rectangle)
            float dx = x2 - x1;
            float dy = y2 - y1;
            float len = std::sqrt(dx * dx + dy * dy);
            
            if (len < 0.001f) return;
            
            float nx = -dy / len * thickness * 0.5f;
            float ny = dx / len * thickness * 0.5f;
            
            // Draw as filled rectangle
            DrawFilledRect(x1 + nx, y1 + ny, x2 - x1 + thickness, y2 - y1 + thickness, color);
        }
    }
    
    // Draw a hollow rectangle
    void DrawRect(float x, float y, float w, float h, 
                 const D3DXCOLOR& color, float thickness = 1.0f) {
        if (thickness <= 1.0f) {
            // Simple outline
            AddLineVertex(ScreenToClipX(x), ScreenToClipY(y), color);
            AddLineVertex(ScreenToClipX(x + w), ScreenToClipY(y), color);
            
            AddLineVertex(ScreenToClipX(x + w), ScreenToClipY(y), color);
            AddLineVertex(ScreenToClipX(x + w), ScreenToClipY(y + h), color);
            
            AddLineVertex(ScreenToClipX(x + w), ScreenToClipY(y + h), color);
            AddLineVertex(ScreenToClipX(x), ScreenToClipY(y + h), color);
            
            AddLineVertex(ScreenToClipX(x), ScreenToClipY(y + h), color);
            AddLineVertex(ScreenToClipX(x), ScreenToClipY(y), color);
        } else {
            // Thick outline (draw as 4 rectangles)
            DrawFilledRect(x, y, w, thickness, color); // Top
            DrawFilledRect(x, y + h - thickness, w, thickness, color); // Bottom
            DrawFilledRect(x, y, thickness, h, color); // Left
            DrawFilledRect(x + w - thickness, y, thickness, h, color); // Right
        }
    }
    
    // Draw a filled rectangle
    void DrawFilledRect(float x, float y, float w, float h, const D3DXCOLOR& color) {
        // Triangle strip for filled rectangle
        AddFilledVertex(ScreenToClipX(x), ScreenToClipY(y), color);
        AddFilledVertex(ScreenToClipX(x), ScreenToClipY(y + h), color);
        AddFilledVertex(ScreenToClipX(x + w), ScreenToClipY(y), color);
        AddFilledVertex(ScreenToClipX(x + w), ScreenToClipY(y + h), color);
    }
    
    // Draw a circle
    void DrawCircle(float centerX, float centerY, float radius, 
                    const D3DXCOLOR& color, int segments = 32, float thickness = 1.0f) {
        if (thickness <= 1.0f) {
            // Draw as line loop
            for (int i = 0; i < segments; i++) {
                float angle1 = (float)i / segments * 6.28318530718f;
                float angle2 = (float)(i + 1) / segments * 6.28318530718f;
                
                float x1 = centerX + std::cos(angle1) * radius;
                float y1 = centerY + std::sin(angle1) * radius;
                float x2 = centerX + std::cos(angle2) * radius;
                float y2 = centerY + std::sin(angle2) * radius;
                
                AddLineVertex(ScreenToClipX(x1), ScreenToClipY(y1), color);
                AddLineVertex(ScreenToClipX(x2), ScreenToClipY(y2), color);
            }
        } else {
            // Thick circle (draw as filled ring)
            for (int i = 0; i < segments; i++) {
                float angle1 = (float)i / segments * 6.28318530718f;
                float angle2 = (float)(i + 1) / segments * 6.28318530718f;
                
                float x1 = centerX + std::cos(angle1) * radius;
                float y1 = centerY + std::sin(angle1) * radius;
                float x2 = centerX + std::cos(angle2) * radius;
                float y2 = centerY + std::sin(angle2) * radius;
                
                float x1_inner = centerX + std::cos(angle1) * (radius - thickness);
                float y1_inner = centerY + std::sin(angle1) * (radius - thickness);
                float x2_inner = centerX + std::cos(angle2) * (radius - thickness);
                float y2_inner = centerY + std::sin(angle2) * (radius - thickness);
                
                // Draw quad for each segment
                AddFilledVertex(ScreenToClipX(x1_inner), ScreenToClipY(y1_inner), color);
                AddFilledVertex(ScreenToClipX(x1), ScreenToClipY(y1), color);
                AddFilledVertex(ScreenToClipX(x2_inner), ScreenToClipY(y2_inner), color);
                AddFilledVertex(ScreenToClipX(x2), ScreenToClipY(y2), color);
            }
        }
    }
    
    // Draw a filled circle
    void DrawFilledCircle(float centerX, float centerY, float radius, 
                         const D3DXCOLOR& color, int segments = 32) {
        for (int i = 0; i < segments; i++) {
            float angle1 = (float)i / segments * 6.28318530718f;
            float angle2 = (float)(i + 1) / segments * 6.28318530718f;
            
            float x1 = centerX + std::cos(angle1) * radius;
            float y1 = centerY + std::sin(angle1) * radius;
            float x2 = centerX + std::cos(angle2) * radius;
            float y2 = centerY + std::sin(angle2) * radius;
            
            // Triangle fan (center to edge)
            AddFilledVertex(ScreenToClipX(centerX), ScreenToClipY(centerY), color);
            AddFilledVertex(ScreenToClipX(x1), ScreenToClipY(y1), color);
            AddFilledVertex(ScreenToClipX(x2), ScreenToClipY(y2), color);
        }
    }
    
    // Draw corner box (modern sleek box with only 4 corners)
    void DrawCornerBox(float x, float y, float w, float h, 
                      const D3DXCOLOR& color, float cornerLength = 10.0f, 
                      float thickness = 2.0f) {
        // Clamp corner length
        float cl = cornerLength;
        if (cl > w / 2) cl = w / 2;
        if (cl > h / 2) cl = h / 2;
        
        // Top-left corner
        DrawLine(x, y, x + cl, y, color, thickness);
        DrawLine(x, y, x, y + cl, color, thickness);
        
        // Top-right corner
        DrawLine(x + w - cl, y, x + w, y, color, thickness);
        DrawLine(x + w, y, x + w, y + cl, color, thickness);
        
        // Bottom-left corner
        DrawLine(x, y + h - cl, x, y + h, color, thickness);
        DrawLine(x, y + h, x + cl, y + h, color, thickness);
        
        // Bottom-right corner
        DrawLine(x + w - cl, y + h, x + w, y + h, color, thickness);
        DrawLine(x + w, y + h - cl, x + w, y + h, color, thickness);
    }
    
    // Draw gradient bar (health bar with gradient)
    void DrawGradientBar(float x, float y, float w, float h, 
                        float value, float maxValue,
                        const D3DXCOLOR& fullColor, const D3DXCOLOR& emptyColor) {
        // Clamp value
        if (value < 0) value = 0;
        if (value > maxValue) value = maxValue;
        
        float percentage = value / maxValue;
        
        // Draw background (empty color)
        DrawFilledRect(x, y, w, h, emptyColor);
        
        if (percentage > 0) {
            // Calculate fill width
            float fillWidth = w * percentage;
            
            // Calculate gradient color based on percentage
            D3DXCOLOR fillColor;
            if (percentage > 0.5f) {
                // Green to yellow
                float t = (percentage - 0.5f) * 2.0f; // 0 to 1
                fillColor = D3DXCOLOR(
                    fullColor.r * (1.0f - t) + 1.0f * t,
                    fullColor.g,
                    fullColor.b,
                    fullColor.a
                );
            } else {
                // Yellow to red
                float t = percentage * 2.0f; // 0 to 1
                fillColor = D3DXCOLOR(
                    1.0f,
                    t,
                    0.0f,
                    fullColor.a
                );
            }
            
            // Draw filled portion
            DrawFilledRect(x, y, fillWidth, h, fillColor);
        }
        
        // Draw border
        DrawRect(x, y, w, h, D3DXCOLOR(1, 1, 1, 0.5f), 1.0f);
    }
    
    // Flush all batched geometry
    void Flush() {
        // Set render state
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        context->IASetInputLayout(inputLayout);
        
        float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        context->OMSetBlendState(blendState, blendFactor, 0xFFFFFFFF);
        
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);
        
        UINT stride = sizeof(VertexPositionColor);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        
        // Draw lines
        if (!lineVertices.empty()) {
            D3D11_MAPPED_SUBRESOURCE ms;
            if (SUCCEEDED(context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
                size_t copySize = lineVertices.size() * sizeof(VertexPositionColor);
                if (copySize <= MAX_VERTICES * sizeof(VertexPositionColor)) {
                    memcpy(ms.pData, lineVertices.data(), copySize);
                }
                context->Unmap(vertexBuffer, 0);
                
                context->Draw(lineVertices.size(), 0);
            }
        }
        
        // Draw filled geometry
        if (!filledVertices.empty()) {
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            
            D3D11_MAPPED_SUBRESOURCE ms;
            if (SUCCEEDED(context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
                size_t copySize = filledVertices.size() * sizeof(VertexPositionColor);
                if (copySize <= MAX_VERTICES * sizeof(VertexPositionColor)) {
                    memcpy(ms.pData, filledVertices.data(), copySize);
                }
                context->Unmap(vertexBuffer, 0);
                
                context->Draw(filledVertices.size(), 0);
            }
        }
        
        // Clear batches
        lineVertices.clear();
        filledVertices.clear();
    }
    
    // Clear batches without drawing
    void Clear() {
        lineVertices.clear();
        filledVertices.clear();
    }
};
