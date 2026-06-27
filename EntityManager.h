#pragma once

#include "ProcessMemory.h"
#include "CS2Entities.h"
#include "Offsets.h"
#include <vector>
#include <string>
#include <map>
#include <algorithm>

struct PlayerData {
    int index;
    int health;
    int team;
    int armor;
    Vector3 position;
    Vector3 headPosition;
    std::string name;
    bool isAlive;
    bool isDormant;
    float distance;
    uintptr_t pawnAddr;
    uintptr_t controllerAddr;
    int entityIndex;
    float flashAlpha;
    bool isDefusing;
};

class EntityManager {
private:
    ProcessMemory& process;
    
    // Cache of player data
    std::vector<PlayerData> players;
    std::map<uint32_t, uintptr_t> handleToPawnMap;
    
    // Local player data
    PlayerData localPlayer;
    
    // Entity list pointer (GameEntitySystem)
    uintptr_t gameEntitySystem;
    
    // Read entity from sparse array using bit-shift method
    uintptr_t GetEntityFromSparseArray(uintptr_t entityListPtr, int index) {
        try {
            // Sparse array traversal: listEntry = entityListPtr + 0x40 + ((index >> 9) * 0x8)
            uintptr_t listEntry = entityListPtr + 0x40 + ((index >> 9) * 0x8);
            
            // Read the list entry pointer
            uintptr_t entryListPtr = process.ReadMemory<uintptr_t>(listEntry);
            if (entryListPtr == 0) return 0;
            
            // Read entity pointer: entryListPtr + 0x8 + ((index & 0x1FF) * 0x78)
            // Note: Some sources use 0x10 instead of 0x8, and 0x78 or 0x10 for stride
            uintptr_t entityPtr = process.ReadMemory<uintptr_t>(
                entryListPtr + 0x8 + ((index & 0x1FF) * 0x78)
            );
            
            return entityPtr;
        } catch (...) {
            return 0;
        }
    }
    
    // Alternative simple method (fallback)
    uintptr_t GetEntitySimple(uintptr_t entityListPtr, int index) {
        try {
            uintptr_t entityPtr = process.ReadMemory<uintptr_t>(
                entityListPtr + 0x40 + (index * 0x10) + 0x10
            );
            return entityPtr;
        } catch (...) {
            return 0;
        }
    }
    
    // Parse entity handle to get index and serial
    void ParseEntityHandle(uint32_t handle, int& outIndex, int& outSerial) {
        outIndex = handle & 0x1FF;
        outSerial = (handle >> 11) & 0x1FFF;
    }
    
    // Get pawn from handle
    uintptr_t GetPawnFromHandle(uint32_t handle) {
        int index, serial;
        ParseEntityHandle(handle, index, serial);
        
        uintptr_t entityPtr = GetEntityFromSparseArray(gameEntitySystem, index);
        if (entityPtr == 0) return 0;
        
        // Verify serial number matches
        // Note: Serial verification depends on implementation
        return entityPtr;
    }
    
    // Read player name from controller
    std::string GetPlayerName(uintptr_t controllerAddr) {
        if (controllerAddr == 0) return "Unknown";
        
        try {
            // m_sSanitizedPlayerName at offset 0x778 (char[16] or similar)
            char nameBuffer[64];
            for (int i = 0; i < 63; i++) {
                nameBuffer[i] = process.ReadMemory<char>(controllerAddr + Offsets::schema::m_sSanitizedPlayerName + i);
                if (nameBuffer[i] == 0) break;
            }
            nameBuffer[63] = 0;
            return std::string(nameBuffer);
        } catch (...) {
            return "Unknown";
        }
    }
    
    // Read controller for a pawn
    uintptr_t GetControllerForPawn(uintptr_t pawnAddr) {
        if (pawnAddr == 0) return 0;
        
        try {
            // This requires iterating the entity list to find matching controller
            // For now, return 0 - this would need full entity list iteration
            return 0;
        } catch (...) {
            return 0;
        }
    }

public:
    EntityManager(ProcessMemory& pm) : process(pm), gameEntitySystem(0) {
        // Initialize local player
        localPlayer = PlayerData{};
    }
    
    // Update entity list and cache player data
    void Update() {
        players.clear();
        handleToPawnMap.clear();
        
        // Read GameEntitySystem pointer
        gameEntitySystem = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwEntityList
        );
        
        if (gameEntitySystem == 0) return;
        
        // Read highest entity index
        int highestIndex = process.ReadMemory<int>(
            gameEntitySystem + Offsets::client_dll::dwGameEntitySystem_highestEntityIndex
        );
        
        // Read local player pawn
        uintptr_t localPawnAddr = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwLocalPlayerPawn
        );
        
        // Read local player controller
        uintptr_t localControllerAddr = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwLocalPlayerController
        );
        
        // Cache local player data
        if (localPawnAddr != 0) {
            Entity localPawn(process, localPawnAddr);
            localPlayer.index = -1; // Local player
            localPlayer.health = localPawn.GetHealth();
            localPlayer.team = localPawn.GetTeam();
            localPlayer.armor = localPawn.GetArmor();
            localPlayer.position = localPawn.GetPosition();
            localPlayer.headPosition = localPawn.GetHeadPosition();
            localPlayer.isAlive = localPawn.IsAlive();
            localPlayer.isDormant = localPawn.IsDormant();
            localPlayer.pawnAddr = localPawnAddr;
            localPlayer.controllerAddr = localControllerAddr;
            localPlayer.entityIndex = localPawn.GetEntityIndex();
            localPlayer.flashAlpha = localPawn.GetFlashAlpha();
            localPlayer.name = "Local Player";
            
            if (localControllerAddr != 0) {
                localPlayer.name = GetPlayerName(localControllerAddr);
            }
        }
        
        // Iterate through all entities
        for (int i = 0; i <= highestIndex && i < 1024; i++) {
            // Try sparse array method first
            uintptr_t entityPtr = GetEntityFromSparseArray(gameEntitySystem, i);
            
            // Fallback to simple method if sparse fails
            if (entityPtr == 0) {
                entityPtr = GetEntitySimple(gameEntitySystem, i);
            }
            
            if (entityPtr == 0) continue;
            
            // Create entity object
            Entity entity(process, entityPtr);
            
            // Read basic data
            int health = entity.GetHealth();
            int team = entity.GetTeam();
            bool dormant = entity.IsDormant();
            int lifeState = entity.GetLifeState();
            
            // Skip invalid entities
            if (health <= 0) continue;
            if (dormant) continue;
            if (lifeState != 0) continue;
            
            // Read position
            Vector3 position = entity.GetPosition();
            if (position.Length() < 0.1f) continue; // Invalid position
            
            // Read head position from bone matrix
            Vector3 headPosition = entity.GetHeadPosition();
            
            // Try to find associated controller for name
            uintptr_t controllerAddr = GetControllerForPawn(entityPtr);
            std::string name = GetPlayerName(controllerAddr);
            
            // Calculate distance from local player
            float distance = 0;
            if (localPlayer.pawnAddr != 0) {
                distance = localPlayer.position.Distance(position);
            }
            
            // Create player data
            PlayerData data;
            data.index = i;
            data.health = health;
            data.team = team;
            data.armor = entity.GetArmor();
            data.position = position;
            data.headPosition = headPosition;
            data.name = name;
            data.isAlive = true;
            data.isDormant = false;
            data.distance = distance;
            data.pawnAddr = entityPtr;
            data.controllerAddr = controllerAddr;
            data.entityIndex = entity.GetEntityIndex();
            data.flashAlpha = entity.GetFlashAlpha();
            
            // Read defusing state
            try {
                data.isDefusing = process.ReadMemory<bool>(
                    entityPtr + Offsets::schema::m_bIsDefusing
                );
            } catch (...) {
                data.isDefusing = false;
            }
            
            players.push_back(data);
        }
    }
    
    // Get local player pawn
    Entity GetLocalPawn() {
        uintptr_t localPawnAddr = process.ReadMemory<uintptr_t>(
            process.GetClientDllBase() + Offsets::client_dll::dwLocalPlayerPawn
        );
        return Entity(process, localPawnAddr);
    }
    
    // Get local player data
    const PlayerData& GetLocalPlayer() const {
        return localPlayer;
    }
    
    // Get all valid targets (alive, non-dormant enemies)
    std::vector<PlayerData> GetValidTargets() {
        std::vector<PlayerData> targets;
        
        int localTeam = localPlayer.team;
        
        for (const auto& player : players) {
            // Skip teammates
            if (player.team == localTeam) continue;
            
            // Skip invalid
            if (!player.isAlive) continue;
            if (player.isDormant) continue;
            
            targets.push_back(player);
        }
        
        return targets;
    }
    
    // Get all players (including teammates)
    std::vector<PlayerData> GetAllPlayers() {
        return players;
    }
    
    // Get players by team
    std::vector<PlayerData> GetPlayersByTeam(int teamNum) {
        std::vector<PlayerData> teamPlayers;
        
        for (const auto& player : players) {
            if (player.team == teamNum) {
                teamPlayers.push_back(player);
            }
        }
        
        return teamPlayers;
    }
    
    // Get player by entity index
    PlayerData* GetPlayerByIndex(int index) {
        for (auto& player : players) {
            if (player.index == index) {
                return &player;
            }
        }
        return nullptr;
    }
    
    // Get player by entity index (from m_iIDEntIndex)
    PlayerData* GetPlayerByEntityIndex(int entityIndex) {
        for (auto& player : players) {
            if (player.entityIndex == entityIndex) {
                return &player;
            }
        }
        return nullptr;
    }
    
    // Sort players by distance
    void SortByDistance() {
        std::sort(players.begin(), players.end(), 
            [](const PlayerData& a, const PlayerData& b) {
                return a.distance < b.distance;
            });
    }
    
    // Sort players by health
    void SortByHealth() {
        std::sort(players.begin(), players.end(),
            [](const PlayerData& a, const PlayerData& b) {
                return a.health < b.health;
            });
    }
};
