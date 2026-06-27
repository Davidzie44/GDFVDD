#pragma once

#include <windows.h>
#include "ProcessMemory.h"
#include "Offsets.h"
#include "CS2Entities.h"

struct Matrix4x4 {
    float m[4][4];

    Matrix4x4() {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                m[i][j] = 0.0f;
            }
        }
    }

    float* operator[](int row) {
        return m[row];
    }

    const float* operator[](int row) const {
        return m[row];
    }
};

class WorldToScreen {
private:
    const ProcessMemory& process;
    Matrix4x4 viewMatrix;
    int screenWidth;
    int screenHeight;
    Vector3 cameraPosition;

    bool ReadViewMatrix() {
        uintptr_t matrixAddress = process.GetClientDllBase() + Offsets::client_dll::dwViewMatrix;
        
        try {
            // Read the 4x4 matrix (16 floats)
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    viewMatrix.m[i][j] = process.ReadMemory<float>(matrixAddress + (i * 4 + j) * sizeof(float));
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    void ExtractCameraPosition() {
        // Camera position is the translation component of the view matrix
        // In a view matrix, the camera position can be extracted from the 4th column
        // For standard view matrices: cameraPos = -transpose(rotation) * translation
        
        // Extract translation from the 4th column (indices [0][3], [1][3], [2][3])
        Vector3 translation(viewMatrix.m[0][3], viewMatrix.m[1][3], viewMatrix.m[2][3]);
        
        // Extract rotation (3x3 upper-left)
        Vector3 right(viewMatrix.m[0][0], viewMatrix.m[1][0], viewMatrix.m[2][0]);
        Vector3 up(viewMatrix.m[0][1], viewMatrix.m[1][1], viewMatrix.m[2][1]);
        Vector3 forward(viewMatrix.m[0][2], viewMatrix.m[1][2], viewMatrix.m[2][2]);
        
        // Camera position = -(right * tx + up * ty + forward * tz)
        cameraPosition.x = -(right.x * translation.x + up.x * translation.y + forward.x * translation.z);
        cameraPosition.y = -(right.y * translation.x + up.y * translation.y + forward.y * translation.z);
        cameraPosition.z = -(right.z * translation.x + up.z * translation.y + forward.z * translation.z);
    }

public:
    WorldToScreen(const ProcessMemory& pm, int width = 1920, int height = 1080) 
        : process(pm), screenWidth(width), screenHeight(height) {
        Update();
    }

    void Update() {
        ReadViewMatrix();
        ExtractCameraPosition();
    }

    void SetScreenSize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
    }

    Vector3 GetCameraPosition() const {
        return cameraPosition;
    }

    bool WorldToScreenPoint(const Vector3& worldPos, Vector3& screenPos) const {
        // Transform world position by view-projection matrix
        // w = world.x * m[0][3] + world.y * m[1][3] + world.z * m[2][3] + m[3][3]
        float w = worldPos.x * viewMatrix.m[0][3] + 
                  worldPos.y * viewMatrix.m[1][3] + 
                  worldPos.z * viewMatrix.m[2][3] + 
                  viewMatrix.m[3][3];

        // Check if point is behind the camera
        if (w < 0.001f) {
            return false;
        }

        // Calculate screen coordinates
        // x = world.x * m[0][0] + world.y * m[1][0] + world.z * m[2][0] + m[3][0]
        float x = worldPos.x * viewMatrix.m[0][0] + 
                  worldPos.y * viewMatrix.m[1][0] + 
                  worldPos.z * viewMatrix.m[2][0] + 
                  viewMatrix.m[3][0];

        // y = world.x * m[0][1] + world.y * m[1][1] + world.z * m[2][1] + m[3][1]
        float y = worldPos.x * viewMatrix.m[0][1] + 
                  worldPos.y * viewMatrix.m[1][1] + 
                  worldPos.z * viewMatrix.m[2][1] + 
                  viewMatrix.m[3][1];

        // Perspective divide
        x /= w;
        y /= w;

        // Convert to screen coordinates
        // Screen center is (screenWidth/2, screenHeight/2)
        screenPos.x = (screenWidth / 2.0f) + (screenWidth / 2.0f) * x;
        screenPos.y = (screenHeight / 2.0f) - (screenHeight / 2.0f) * y; // Flip Y for screen coords
        screenPos.z = 0.0f;

        // Check if point is on screen
        return screenPos.x >= 0 && screenPos.x <= screenWidth &&
               screenPos.y >= 0 && screenPos.y <= screenHeight;
    }

    bool WorldToScreenPoint(const Vector3& worldPos, float& screenX, float& screenY) const {
        Vector3 screenPos;
        bool result = WorldToScreenPoint(worldPos, screenPos);
        screenX = screenPos.x;
        screenY = screenPos.y;
        return result;
    }

    // Alternative method using standard matrix multiplication
    bool Transform(const Vector3& worldPos, Vector3& screenPos) const {
        // Multiply by matrix: result = world * matrix
        float w = worldPos.x * viewMatrix.m[3][0] + 
                  worldPos.y * viewMatrix.m[3][1] + 
                  worldPos.z * viewMatrix.m[3][2] + 
                  viewMatrix.m[3][3];

        if (w < 0.001f) {
            return false;
        }

        float x = worldPos.x * viewMatrix.m[0][0] + 
                  worldPos.y * viewMatrix.m[0][1] + 
                  worldPos.z * viewMatrix.m[0][2] + 
                  viewMatrix.m[0][3];

        float y = worldPos.x * viewMatrix.m[1][0] + 
                  worldPos.y * viewMatrix.m[1][1] + 
                  worldPos.z * viewMatrix.m[1][2] + 
                  viewMatrix.m[1][3];

        x /= w;
        y /= w;

        screenPos.x = (screenWidth / 2.0f) * (1.0f + x);
        screenPos.y = (screenHeight / 2.0f) * (1.0f - y);
        screenPos.z = 0.0f;

        return screenPos.x >= 0 && screenPos.x <= screenWidth &&
               screenPos.y >= 0 && screenPos.y <= screenHeight;
    }

    const Matrix4x4& GetViewMatrix() const {
        return viewMatrix;
    }
};
