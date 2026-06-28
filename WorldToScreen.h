#pragma once

#include <windows.h>
#include "ProcessMemory.h"
#include "Offsets.h"
#include "CS2Entities.h"

struct Matrix4x4 {
    float m[4][4];

    Matrix4x4() {
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                m[i][j] = 0.0f;
    }

    float* operator[](int row) { return m[row]; }
    const float* operator[](int row) const { return m[row]; }
};

class WorldToScreen {
private:
    const ProcessMemory& process;
    Matrix4x4 viewMatrix;
    int screenWidth;
    int screenHeight;

    bool ReadViewMatrix() {
        uintptr_t addr = process.GetClientDllBase() + Offsets::client_dll::dwViewMatrix;
        try {
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    viewMatrix.m[i][j] = process.ReadMemory<float>(addr + (i * 4 + j) * 4);
            return true;
        } catch (...) { return false; }
    }

public:
    WorldToScreen(const ProcessMemory& pm, int w = 1920, int h = 1080)
        : process(pm), screenWidth(w), screenHeight(h) {
        Update();
    }

    void Update() { ReadViewMatrix(); }

    void SetScreenSize(int w, int h) { screenWidth = w; screenHeight = h; }

    Vector3 GetCameraPosition() const {
        return Vector3(0, 0, 0);
    }

    bool WorldToScreenPoint(const Vector3& worldPos, Vector3& screenPos) const {
        float x = viewMatrix.m[0][0] * worldPos.x + viewMatrix.m[0][1] * worldPos.y + viewMatrix.m[0][2] * worldPos.z + viewMatrix.m[0][3];
        float y = viewMatrix.m[1][0] * worldPos.x + viewMatrix.m[1][1] * worldPos.y + viewMatrix.m[1][2] * worldPos.z + viewMatrix.m[1][3];
        float w = viewMatrix.m[3][0] * worldPos.x + viewMatrix.m[3][1] * worldPos.y + viewMatrix.m[3][2] * worldPos.z + viewMatrix.m[3][3];

        if (w < 0.001f) return false;

        float invW = 1.0f / w;
        x *= invW;
        y *= invW;

        screenPos.x = (screenWidth / 2.0f) + (screenWidth / 2.0f) * x;
        screenPos.y = (screenHeight / 2.0f) - (screenHeight / 2.0f) * y;
        screenPos.z = w;

        return screenPos.x >= -500 && screenPos.x <= screenWidth + 500 &&
               screenPos.y >= -500 && screenPos.y <= screenHeight + 500;
    }

    bool WorldToScreenPoint(const Vector3& worldPos, float& screenX, float& screenY) const {
        Vector3 screenPos;
        bool result = WorldToScreenPoint(worldPos, screenPos);
        screenX = screenPos.x;
        screenY = screenPos.y;
        return result;
    }

    const Matrix4x4& GetViewMatrix() const { return viewMatrix; }
};
