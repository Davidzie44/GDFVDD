#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <stdexcept>
#include <cstdint>

class ProcessMemory {
private:
    HANDLE processHandle;
    DWORD processId;
    uintptr_t clientDllBase;
    uintptr_t engine2DllBase;

    DWORD FindProcessId(const std::wstring& processName) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to create process snapshot");
        }

        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(PROCESSENTRY32W);

        if (!Process32FirstW(snapshot, &entry)) {
            CloseHandle(snapshot);
            throw std::runtime_error("Failed to retrieve first process");
        }

        DWORD pid = 0;
        do {
            if (processName == entry.szExeFile) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));

        CloseHandle(snapshot);
        return pid;
    }

    uintptr_t FindModuleBase(DWORD pid, const std::wstring& moduleName) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to create module snapshot");
        }

        MODULEENTRY32W entry;
        entry.dwSize = sizeof(MODULEENTRY32W);

        if (!Module32FirstW(snapshot, &entry)) {
            CloseHandle(snapshot);
            throw std::runtime_error("Failed to retrieve first module");
        }

        uintptr_t baseAddress = 0;
        do {
            if (moduleName == entry.szModule) {
                baseAddress = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry));

        CloseHandle(snapshot);
        return baseAddress;
    }

public:
    ProcessMemory() : processHandle(INVALID_HANDLE_VALUE), processId(0), 
                      clientDllBase(0), engine2DllBase(0) {}

    ~ProcessMemory() {
        if (processHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(processHandle);
        }
    }

    void Attach(const std::wstring& processName) {
        processId = FindProcessId(processName);
        if (processId == 0) {
            throw std::runtime_error("Process not found: " + std::string(processName.begin(), processName.end()));
        }

        processHandle = OpenProcess(
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            FALSE,
            processId
        );

        if (processHandle == INVALID_HANDLE_VALUE || processHandle == NULL) {
            throw std::runtime_error("Failed to open process handle");
        }

        clientDllBase = FindModuleBase(processId, L"client.dll");
        engine2DllBase = FindModuleBase(processId, L"engine2.dll");
    }

    bool IsAttached() const {
        return processHandle != INVALID_HANDLE_VALUE && processHandle != NULL;
    }

    uintptr_t GetClientDllBase() const {
        if (!IsAttached()) {
            throw std::runtime_error("Process not attached");
        }
        return clientDllBase;
    }

    uintptr_t GetEngine2DllBase() const {
        if (!IsAttached()) {
            throw std::runtime_error("Process not attached");
        }
        return engine2DllBase;
    }

    template<typename T>
    T ReadMemory(uintptr_t address) const {
        if (!IsAttached()) {
            throw std::runtime_error("Invalid process handle");
        }

        T value;
        SIZE_T bytesRead;
        BOOL result = ReadProcessMemory(
            processHandle,
            reinterpret_cast<LPCVOID>(address),
            &value,
            sizeof(T),
            &bytesRead
        );

        if (!result || bytesRead != sizeof(T)) {
            throw std::runtime_error("Failed to read memory");
        }

        return value;
    }

    template<typename T>
    void WriteMemory(uintptr_t address, const T& value) const {
        if (!IsAttached()) {
            throw std::runtime_error("Invalid process handle");
        }

        SIZE_T bytesWritten;
        BOOL result = WriteProcessMemory(
            processHandle,
            reinterpret_cast<LPVOID>(address),
            &value,
            sizeof(T),
            &bytesWritten
        );

        if (!result || bytesWritten != sizeof(T)) {
            throw std::runtime_error("Failed to write memory");
        }
    }

    template<typename T>
    bool TryReadMemory(uintptr_t address, T& outValue) const noexcept {
        if (!IsAttached()) {
            return false;
        }

        SIZE_T bytesRead;
        return ReadProcessMemory(
            processHandle,
            reinterpret_cast<LPCVOID>(address),
            &outValue,
            sizeof(T),
            &bytesRead
        ) && bytesRead == sizeof(T);
    }

    template<typename T>
    bool TryWriteMemory(uintptr_t address, const T& value) const noexcept {
        if (!IsAttached()) {
            return false;
        }

        SIZE_T bytesWritten;
        return WriteProcessMemory(
            processHandle,
            reinterpret_cast<LPVOID>(address),
            &value,
            sizeof(T),
            &bytesWritten
        ) && bytesWritten == sizeof(T);
    }
};
