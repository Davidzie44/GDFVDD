#pragma once

#include <windows.h>
#include <cstdint>
#include <cmath>
#include <vector>
#include <fstream>
#include "ProcessMemory.h"
#include "CS2Entities.h"

extern std::ofstream g_log;
void LogFmt(const char* fmt, ...);

struct TraceData {
    uint64_t engineTracePtr;
    float startX, startY, startZ;
    float endX, endY, endZ;
    float fraction;
    uint32_t hitEntity;
};

class VisibilityCheck {
private:
    ProcessMemory& process;
    uintptr_t engineTraceAddr = 0;
    HANDLE hProcess = nullptr;
    LPVOID remoteShellcode = nullptr;
    LPVOID remoteData = nullptr;
    bool initialized = false;

    uintptr_t FindModuleSize(DWORD pid, const wchar_t* modName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return 0x1000000;
        MODULEENTRY32W me = { sizeof(me) };
        uintptr_t size = 0x1000000;
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, modName) == 0) {
                    size = me.modBaseSize;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return size;
    }

    uintptr_t PatternScan(HANDLE hp, uintptr_t base, size_t size, const uint8_t* pattern, const char* mask) {
        size_t maskLen = strlen(mask);
        if (maskLen == 0 || size < maskLen) return 0;

        std::vector<uint8_t> buf(size);
        SIZE_T read = 0;
        if (!ReadProcessMemory(hp, (LPCVOID)base, buf.data(), size, &read) || read < maskLen)
            return 0;

        for (size_t i = 0; i <= size - maskLen; i++) {
            bool ok = true;
            for (size_t j = 0; j < maskLen; j++) {
                if (mask[j] == 'x' && buf[i + j] != pattern[j]) {
                    ok = false;
                    break;
                }
            }
            if (ok) return base + i;
        }
        return 0;
    }

    uintptr_t FindEngineTrace() {
        uintptr_t base = process.GetEngine2DllBase();
        if (base == 0) return 0;
        size_t modSize = FindModuleSize(process.GetProcessId(), L"engine2.dll");

        // Pattern 1: mov rcx,[rip+??]; mov rax,[rcx]; call [rax+0x28]
        {
            const uint8_t pat[] = { 0x48,0x8B,0x0D,0,0,0,0,0x48,0x8B,0x01,0xFF,0x50,0x28 };
            const char msk[] = "xxx????xxxxxx";
            uintptr_t hit = PatternScan(hProcess, base, modSize, pat, msk);
            if (hit) {
                int32_t off = 0;
                ReadProcessMemory(hProcess, (LPCVOID)(hit + 3), &off, 4, nullptr);
                uintptr_t addr = hit + 7 + off;
                uintptr_t ptr = 0;
                ReadProcessMemory(hProcess, (LPCVOID)addr, &ptr, 8, nullptr);
                if (ptr) {
                    LogFmt("Vis: EngineTrace found (pat1) addr=0x%llX ptr=0x%llX",
                        (unsigned long long)addr, (unsigned long long)ptr);
                    return ptr;
                }
            }
        }

        // Pattern 2: mov rax,[rip+??]; mov rcx,[rax]; call [rcx+0x28]
        {
            const uint8_t pat[] = { 0x48,0x8B,0x05,0,0,0,0,0x48,0x8B,0x08,0xFF,0x51,0x28 };
            const char msk[] = "xxx????xxxxxx";
            uintptr_t hit = PatternScan(hProcess, base, modSize, pat, msk);
            if (hit) {
                int32_t off = 0;
                ReadProcessMemory(hProcess, (LPCVOID)(hit + 3), &off, 4, nullptr);
                uintptr_t addr = hit + 7 + off;
                uintptr_t ptr = 0;
                ReadProcessMemory(hProcess, (LPCVOID)addr, &ptr, 8, nullptr);
                if (ptr) {
                    LogFmt("Vis: EngineTrace found (pat2) addr=0x%llX ptr=0x%llX",
                        (unsigned long long)addr, (unsigned long long)ptr);
                    return ptr;
                }
            }
        }

        // Pattern 3: lea rcx,[rip+??]; mov rax,[rcx]; call [rax+0x28]
        {
            const uint8_t pat[] = { 0x48,0x8D,0x0D,0,0,0,0,0x48,0x8B,0x01,0xFF,0x50,0x28 };
            const char msk[] = "xxx????xxxxxx";
            uintptr_t hit = PatternScan(hProcess, base, modSize, pat, msk);
            if (hit) {
                int32_t off = 0;
                ReadProcessMemory(hProcess, (LPCVOID)(hit + 3), &off, 4, nullptr);
                uintptr_t addr = hit + 7 + off;
                uintptr_t ptr = 0;
                ReadProcessMemory(hProcess, (LPCVOID)addr, &ptr, 8, nullptr);
                if (ptr) {
                    LogFmt("Vis: EngineTrace found (pat3) addr=0x%llX ptr=0x%llX",
                        (unsigned long long)addr, (unsigned long long)ptr);
                    return ptr;
                }
            }
        }

        LogFmt("Vis: EngineTrace NOT FOUND in engine2.dll");
        return 0;
    }

    bool Setup() {
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process.GetProcessId());
        if (!hProcess) {
            LogFmt("Vis: OpenProcess failed err=%lu", GetLastError());
            return false;
        }

        engineTraceAddr = FindEngineTrace();
        if (!engineTraceAddr) return false;

        remoteData = VirtualAllocEx(hProcess, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        remoteShellcode = VirtualAllocEx(hProcess, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!remoteData || !remoteShellcode) {
            LogFmt("Vis: VirtualAllocEx failed err=%lu", GetLastError());
            return false;
        }

        /*
         * x64 shellcode: void __fastcall TraceFunc(TraceData* pData)
         * rcx = pData
         *
         * Stack layout (after sub rsp, 0xC0):
         *   [rsp+0x00..0x1F] = shadow space for TraceRay call
         *   [rsp+0x20..0x5F] = Ray_t (VectorAligned = 16 bytes each)
         *     +0x20: start    (16 bytes, only 12 used)
         *     +0x30: delta    (16 bytes)
         *     +0x40: startOff (16 bytes, zeroed)
         *     +0x50: extents  (16 bytes, zeroed)
         *     +0x60: isRay(1), isSwept(1)
         *   [rsp+0x68..0xB0] = CGameTrace (0x48 bytes)
         *     +0x68+0x00: startpos (12)
         *     +0x68+0x0C: endpos   (12)
         *     +0x68+0x18: plane    (20)
         *     +0x68+0x2C: fraction (4)  -> rsp+0x94
         *     +0x68+0x30: contents (4)
         *     +0x68+0x34: dispFlags(2)
         *     +0x68+0x36: allsolid (1)
         *     +0x68+0x37: startsolid(1)
         *     +0x68+0x38: fracLeft (4)
         *     +0x68+0x3C: ent      (4)  -> rsp+0xA4
         *     +0x68+0x40: hitbox   (4)
         *     +0x68+0x44: hitgroup (4)
         *
         * TraceRay(this=rcx, &ray=rdx, mask=r8d, filter=r9, &trace=[rsp+0x20])
         */

        static const uint8_t code[] = {
            // Prologue
            0x53,                                           // push rbx
            0x56,                                           // push rsi
            0x57,                                           // push rdi
            0x48, 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00,      // sub rsp, 0xC0

            0x48, 0x89, 0xCB,                               // mov rbx, rcx (pData)

            // Zero Ray_t at [rsp+0x20] (0x42 bytes -> zero 0x48 = 72 bytes)
            0x31, 0xC0,                                     // xor eax, eax
            0x48, 0x8D, 0x7C, 0x24, 0x20,                   // lea rdi, [rsp+0x20]
            0xB9, 0x48, 0x00, 0x00, 0x00,                   // mov ecx, 0x48
            0xF3, 0xAA,                                     // rep stosb

            // Zero CGameTrace at [rsp+0x68] (0x48 bytes)
            0x48, 0x8D, 0x7C, 0x24, 0x68,                   // lea rdi, [rsp+0x68]
            0xB9, 0x48, 0x00, 0x00, 0x00,                   // mov ecx, 0x48
            0xF3, 0xAA,                                     // rep stosb

            // Fill Ray_t.m_Start = pData->start
            0xF3, 0x0F, 0x10, 0x43, 0x08,                   // movss xmm0, [rbx+0x08]
            0xF3, 0x0F, 0x11, 0x44, 0x24, 0x20,             // movss [rsp+0x20], xmm0
            0xF3, 0x0F, 0x10, 0x43, 0x0C,                   // movss xmm0, [rbx+0x0C]
            0xF3, 0x0F, 0x11, 0x44, 0x24, 0x24,             // movss [rsp+0x24], xmm0
            0xF3, 0x0F, 0x10, 0x43, 0x10,                   // movss xmm0, [rbx+0x10]
            0xF3, 0x0F, 0x11, 0x44, 0x24, 0x28,             // movss [rsp+0x28], xmm0

            // Fill Ray_t.m_Delta = end - start
            0xF3, 0x0F, 0x10, 0x43, 0x14,                   // movss xmm0, [rbx+0x14]
            0xF3, 0x0F, 0x5C, 0x43, 0x08,                   // subss xmm0, [rbx+0x08]
            0xF3, 0x0F, 0x11, 0x44, 0x24, 0x30,             // movss [rsp+0x30], xmm0

            0xF3, 0x0F, 0x10, 0x43, 0x18,                   // movss xmm0, [rbx+0x18]
            0xF3, 0x0F, 0x5C, 0x43, 0x0C,                   // subss xmm0, [rbx+0x0C]
            0xF3, 0x0F, 0x11, 0x44, 0x24, 0x34,             // movss [rsp+0x34], xmm0

            0xF3, 0x0F, 0x10, 0x43, 0x1C,                   // movss xmm0, [rbx+0x1C]
            0xF3, 0x0F, 0x5C, 0x43, 0x10,                   // subss xmm0, [rbx+0x10]
            0xF3, 0x0F, 0x11, 0x44, 0x24, 0x38,             // movss [rsp+0x38], xmm0

            // m_IsRay=1 at rsp+0x20+0x40=rsp+0x60
            0xC6, 0x44, 0x24, 0x60, 0x01,                   // mov byte [rsp+0x60], 1
            // m_IsSwept=1 at rsp+0x20+0x41=rsp+0x61
            0xC6, 0x44, 0x24, 0x61, 0x01,                   // mov byte [rsp+0x61], 1

            // Call EngineTrace::TraceRay
            0x48, 0x8B, 0x03,                               // mov rax, [rbx]        ; engineTrace
            0x48, 0x89, 0xC1,                               // mov rcx, rax          ; this
            0x48, 0x8D, 0x54, 0x24, 0x20,                   // lea rdx, [rsp+0x20]   ; &Ray_t
            0x41, 0xB8, 0x0B, 0x00, 0x04, 0x02,             // mov r8d, 0x0204000B   ; MASK_SHOT
            0x4D, 0x31, 0xC9,                               // xor r9, r9            ; nullptr filter
            0x48, 0x8D, 0x44, 0x24, 0x68,                   // lea rax, [rsp+0x68]   ; &CGameTrace
            0x48, 0x89, 0x44, 0x24, 0x20,                   // mov [rsp+0x20], rax   ; 5th param

            // Call via vtable: [this] = vtable, [vtable + 5*8] = TraceRay
            0x48, 0x8B, 0x01,                               // mov rax, [rcx]        ; vtable
            0xFF, 0x50, 0x28,                               // call [rax+0x28]       ; TraceRay

            // Read CGameTrace.m_flFraction at [rsp+0x68+0x2C] = [rsp+0x94]
            0xF3, 0x0F, 0x10, 0x84, 0x24, 0x94, 0x00, 0x00, 0x00, // movss xmm0, [rsp+0x94]
            0xF3, 0x0F, 0x11, 0x43, 0x20,                   // movss [rbx+0x20], xmm0 ; fraction

            // Read CGameTrace.m_hEntity at [rsp+0x68+0x3C] = [rsp+0xA4]
            0x8B, 0x84, 0x24, 0xA4, 0x00, 0x00, 0x00,      // mov eax, [rsp+0xA4]
            0x89, 0x43, 0x24,                               // mov [rbx+0x24], eax   ; hitEntity

            // Epilogue
            0x48, 0x81, 0xC4, 0xC0, 0x00, 0x00, 0x00,      // add rsp, 0xC0
            0x5F,                                           // pop rdi
            0x5E,                                           // pop rsi
            0x5B,                                           // pop rbx
            0xC3,                                           // ret
        };

        SIZE_T written = 0;
        if (!WriteProcessMemory(hProcess, remoteShellcode, code, sizeof(code), &written)) {
            LogFmt("Vis: WriteProcessMemory shellcode failed err=%lu", GetLastError());
            return false;
        }

        LogFmt("Vis: Initialized OK. EngineTrace=0x%llX Shellcode=0x%llX (%zu bytes)",
            (unsigned long long)engineTraceAddr,
            (unsigned long long)remoteShellcode,
            sizeof(code));

        initialized = true;
        return true;
    }

public:
    VisibilityCheck(ProcessMemory& pm) : process(pm) {}

    ~VisibilityCheck() {
        if (remoteShellcode && hProcess)
            VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);
        if (remoteData && hProcess)
            VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);
        if (hProcess)
            CloseHandle(hProcess);
    }

    bool Initialize() {
        if (initialized) return true;
        return Setup();
    }

    bool IsVisible(Vector3 eyePos, Vector3 targetPos) {
        if (!initialized) return true;

        TraceData data = {};
        data.engineTracePtr = engineTraceAddr;
        data.startX = eyePos.x;
        data.startY = eyePos.y;
        data.startZ = eyePos.z;
        data.endX = targetPos.x;
        data.endY = targetPos.y;
        data.endZ = targetPos.z;
        data.fraction = 0.0f;
        data.hitEntity = 0;

        if (!WriteProcessMemory(hProcess, remoteData, &data, sizeof(TraceData), nullptr))
            return true;

        HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
            (LPTHREAD_START_ROUTINE)remoteShellcode, remoteData, 0, nullptr);
        if (!hThread) return true;

        WaitForSingleObject(hThread, 100);
        CloseHandle(hThread);

        if (!ReadProcessMemory(hProcess, remoteData, &data, sizeof(TraceData), nullptr))
            return true;

        return data.fraction >= 0.97f;
    }

    bool IsAvailable() const { return initialized; }
};
