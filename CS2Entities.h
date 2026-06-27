#pragma once

#include <windows.h>
#include <cmath>
#include <vector>
#include "ProcessMemory.h"
#include "Offsets.h"

struct Vector3 {
    float x;
    float y;
    float z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    float Distance(const Vector3& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    float Length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    void Normalize() {
        float len = Length();
        if (len > 0.0001f) {
            x /= len;
            y /= len;
            z /= len;
        }
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }
};

struct Vector2 {
    float x;
    float y;

    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}

    float Length() const {
        return std::sqrt(x * x + y * y);
    }

    Vector2 operator+(const Vector2& other) const {
        return Vector2(x + other.x, y + other.y);
    }

    Vector2 operator-(const Vector2& other) const {
        return Vector2(x - other.x, y - other.y);
    }

    Vector2 operator*(float scalar) const {
        return Vector2(x * scalar, y * scalar);
    }
};

struct ViewAngles {
    float pitch;
    float yaw;

    ViewAngles() : pitch(0), yaw(0) {}
    ViewAngles(float p, float y) : pitch(p), yaw(y) {}
    
    void Normalize() {
        // Normalize pitch to -89 to 89
        while (pitch > 89.0f) pitch -= 180.0f;
        while (pitch < -89.0f) pitch += 180.0f;
        
        // Normalize yaw to -180 to 180
        while (yaw > 180.0f) yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;
    }
};

class Entity {
private:
    const ProcessMemory& process;
    uintptr_t address;

public:
    Entity(const ProcessMemory& pm, uintptr_t addr) : process(pm), address(addr) {}

    bool IsValid() const {
        return address != 0;
    }

    // Offsets for entity fields (Build 14165)
    static constexpr uintptr_t m_iHealth = Offsets::schema::m_iHealth;
    static constexpr uintptr_t m_iTeamNum = Offsets::schema::m_iTeamNum;
    static constexpr uintptr_t m_bDormant = Offsets::schema::m_bDormant;
    static constexpr uintptr_t m_vecOrigin = Offsets::schema::m_vecOrigin;
    static constexpr uintptr_t m_lifeState = Offsets::schema::m_lifeState;
    static constexpr uintptr_t m_pGameSceneNode = Offsets::schema::m_pGameSceneNode;
    static constexpr uintptr_t m_iIDEntIndex = Offsets::schema::m_iIDEntIndex;
    static constexpr uintptr_t m_aimPunchAngle = Offsets::schema::m_aimPunchAngle;
    static constexpr uintptr_t m_iShotsFired = Offsets::schema::m_iShotsFired;
    static constexpr uintptr_t m_flFlashOverlayAlpha = Offsets::schema::m_flFlashOverlayAlpha;
    static constexpr uintptr_t m_ArmorValue = Offsets::schema::m_ArmorValue;

    int GetHealth() const {
        if (!IsValid()) return 0;
        try {
            return process.ReadMemory<int>(address + m_iHealth);
        } catch (...) {
            return 0;
        }
    }

    int GetTeam() const {
        if (!IsValid()) return 0;
        try {
            return process.ReadMemory<int>(address + m_iTeamNum);
        } catch (...) {
            return 0;
        }
    }

    bool IsDormant() const {
        if (!IsValid()) return true;
        try {
            return process.ReadMemory<bool>(address + m_bDormant);
        } catch (...) {
            return true;
        }
    }

    Vector3 GetPosition() const {
        if (!IsValid()) return Vector3();
        try {
            float x = process.ReadMemory<float>(address + m_vecOrigin);
            float y = process.ReadMemory<float>(address + m_vecOrigin + 0x4);
            float z = process.ReadMemory<float>(address + m_vecOrigin + 0x8);
            return Vector3(x, y, z);
        } catch (...) {
            return Vector3();
        }
    }

    int GetLifeState() const {
        if (!IsValid()) return -1;
        try {
            return process.ReadMemory<int>(address + m_lifeState);
        } catch (...) {
            return -1;
        }
    }

    bool IsAlive() const {
        return GetHealth() > 0 && GetLifeState() == 0;
    }

    uintptr_t GetAddress() const {
        return address;
    }

    // Get game scene node pointer
    uintptr_t GetGameSceneNode() const {
        if (!IsValid()) return 0;
        try {
            return process.ReadMemory<uintptr_t>(address + m_pGameSceneNode);
        } catch (...) {
            return 0;
        }
    }

    // Get bone position from bone matrix
    Vector3 GetBonePosition(int boneIndex) const {
        uintptr_t gameSceneNode = GetGameSceneNode();
        if (gameSceneNode == 0) return Vector3();

        try {
            // Read model state from game scene node + 0x170
            uintptr_t modelState = process.ReadMemory<uintptr_t>(gameSceneNode + Offsets::schema::m_modelState);
            if (modelState == 0) return Vector3();

            // Read bone array pointer from modelState + 0x80
            uintptr_t boneArray = process.ReadMemory<uintptr_t>(modelState + Offsets::schema::m_boneArray);
            if (boneArray == 0) return Vector3();

            // Each bone is a 3x4 matrix (0x20 bytes)
            // Position is at matrix[0][3], matrix[1][3], matrix[2][3]
            uintptr_t boneMatrix = boneArray + (boneIndex * 0x20);

            float x = process.ReadMemory<float>(boneMatrix + 0x0C); // matrix[0][3]
            float y = process.ReadMemory<float>(boneMatrix + 0x1C); // matrix[1][3]
            float z = process.ReadMemory<float>(boneMatrix + 0x2C); // matrix[2][3]

            return Vector3(x, y, z);
        } catch (...) {
            return Vector3();
        }
    }

    // Get head position (bone 6)
    Vector3 GetHeadPosition() const {
        return GetBonePosition(6);
    }

    // Get aim punch angle for RCS
    Vector2 GetAimPunch() const {
        if (!IsValid()) return Vector2(0, 0);
        try {
            float x = process.ReadMemory<float>(address + m_aimPunchAngle);
            float y = process.ReadMemory<float>(address + m_aimPunchAngle + 0x4);
            return Vector2(x, y);
        } catch (...) {
            return Vector2(0, 0);
        }
    }

    // Get shots fired
    int GetShotsFired() const {
        if (!IsValid()) return 0;
        try {
            return process.ReadMemory<int>(address + m_iShotsFired);
        } catch (...) {
            return 0;
        }
    }

    // Get flash alpha
    float GetFlashAlpha() const {
        if (!IsValid()) return 0;
        try {
            return process.ReadMemory<float>(address + m_flFlashOverlayAlpha);
        } catch (...) {
            return 0;
        }
    }

    // Get armor value
    int GetArmor() const {
        if (!IsValid()) return 0;
        try {
            return process.ReadMemory<int>(address + m_ArmorValue);
        } catch (...) {
            return 0;
        }
    }

    // Get entity index
    int GetEntityIndex() const {
        if (!IsValid()) return -1;
        try {
            return process.ReadMemory<int>(address + m_iIDEntIndex);
        } catch (...) {
            return -1;
        }
    }
};

class EntityList {
private:
    const ProcessMemory& process;
    uintptr_t listAddress;

public:
    EntityList(const ProcessMemory& pm) : process(pm) {
        listAddress = pm.GetClientDllBase() + Offsets::client_dll::dwEntityList;
    }

    uintptr_t GetEntityAddress(int index) const {
        // Entity list entry at: (index * 0x10) + 0x10 from list base
        uintptr_t entryAddress = listAddress + (index * 0x10) + 0x10;
        
        try {
            // Read the entity pointer from the entry
            return process.ReadMemory<uintptr_t>(entryAddress);
        } catch (...) {
            return 0;
        }
    }

    Entity GetEntity(int index) const {
        uintptr_t entityAddress = GetEntityAddress(index);
        return Entity(process, entityAddress);
    }

    // Iterate through all entities and return valid ones
    std::vector<Entity> GetAllEntities(int maxEntities = 64) const {
        std::vector<Entity> entities;
        
        for (int i = 0; i < maxEntities; i++) {
            Entity entity = GetEntity(i);
            if (entity.IsValid() && !entity.IsDormant()) {
                entities.push_back(entity);
            }
        }
        
        return entities;
    }

    // Get only alive players
    std::vector<Entity> GetAlivePlayers(int maxEntities = 64) const {
        std::vector<Entity> players;
        
        for (int i = 0; i < maxEntities; i++) {
            Entity entity = GetEntity(i);
            if (entity.IsValid() && !entity.IsDormant() && entity.IsAlive()) {
                players.push_back(entity);
            }
        }
        
        return players;
    }

    // Get players by team
    std::vector<Entity> GetPlayersByTeam(int teamNum, int maxEntities = 64) const {
        std::vector<Entity> players;
        
        for (int i = 0; i < maxEntities; i++) {
            Entity entity = GetEntity(i);
            if (entity.IsValid() && !entity.IsDormant() && entity.GetTeam() == teamNum) {
                players.push_back(entity);
            }
        }
        
        return players;
    }

    uintptr_t GetListAddress() const {
        return listAddress;
    }
};
