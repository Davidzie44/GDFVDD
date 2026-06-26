#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>

int main(){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    PROCESSENTRY32W e={sizeof(PROCESSENTRY32W)}; DWORD pid=0;
    if(Process32FirstW(s,&e)) do{
        if(_wcsicmp(e.szExeFile,L"aces.exe")==0||_wcsicmp(e.szExeFile,L"launcher.exe")==0){pid=e.th32ProcessID;break;}
    }while(Process32NextW(s,&e)); CloseHandle(s);

    if(!pid){printf("[!] War Thunder not running\n");system("pause");return 1;}
    printf("[+] Found PID: %d\n",pid);

    HANDLE h=OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid);
    if(!h){printf("[!] OpenProcess failed\n");system("pause");return 1;}

    wchar_t path[MAX_PATH];GetFullPathNameW(L"wt_tool.dll",MAX_PATH,path,NULL);
    void* rem=VirtualAllocEx(h,NULL,4096,MEM_COMMIT,PAGE_READWRITE);
    WriteProcessMemory(h,rem,path,4096,NULL);
    HANDLE t=CreateRemoteThread(h,NULL,0,(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"),"LoadLibraryW"),rem,0,NULL);

    if(t){
        printf("[+] Injected wt_tool.dll successfully\n");
        WaitForSingleObject(t,INFINITE);

        // Inject wt-dumper_[unknowncheats.me]_.dll
        wchar_t dumperPath[MAX_PATH];
        GetFullPathNameW(L"wt-dumper_[unknowncheats.me]_.dll",MAX_PATH,dumperPath,NULL);
        void* remDumper=VirtualAllocEx(h,NULL,4096,MEM_COMMIT,PAGE_READWRITE);
        WriteProcessMemory(h,remDumper,dumperPath,4096,NULL);
        HANDLE tDumper=CreateRemoteThread(h,NULL,0,(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"),"LoadLibraryW"),remDumper,0,NULL);

        if(tDumper){
            printf("[+] Injected wt-dumper_[unknowncheats.me]_.dll successfully\n");
            WaitForSingleObject(tDumper,INFINITE);
        } else {
            printf("[!] Injection of wt-dumper_[unknowncheats.me]_.dll failed\n");
        }
        VirtualFreeEx(h,remDumper,0,MEM_RELEASE);

    } else {
        printf("[!] Injection of wt_tool.dll failed\n");
    }

    VirtualFreeEx(h,rem,0,MEM_RELEASE);CloseHandle(h);
    system("pause");return 0;
}
