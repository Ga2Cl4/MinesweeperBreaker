// minecracker.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#pragma comment(lib, "shlwapi")

#include <iostream>
#include <Windows.h>
#include <Shlwapi.h>

#define DLL_NAME            L"minehook.dll"
#define HOOKSTART_NAME      "HookStart"
#define HOOKSTOP_NAME       "HookStop"

#define HOOKED_PROCESS_NAME L"扫雷"

typedef void (*PFN_HOOKSTART)(DWORD dwTid);
typedef void (*PFN_HOOKSTOP)();

int main()
{
    WCHAR errbuf[50] = { 0 };
    HMODULE hDll = LoadLibraryW(DLL_NAME);
    if (!hDll)
    {
        wsprintfW(errbuf, L"Failed to load dll, error : 0x%X", GetLastError());
        system("PAUSE");
        return 1;
    }
    PFN_HOOKSTART HookStart = (PFN_HOOKSTART)GetProcAddress(hDll, HOOKSTART_NAME);
    PFN_HOOKSTOP HookStop = (PFN_HOOKSTOP)GetProcAddress(hDll, HOOKSTOP_NAME);

    // get target process id
    HWND hWnd = FindWindowW(NULL, HOOKED_PROCESS_NAME);
    DWORD pid = 0;
    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, &pid);
    if (!pid)
    {
        MessageBoxW(NULL, L"Failed to get target pid", L"Error", MB_OK);
        system("PAUSE");
        return 1;
    }
    HookStart(dwThreadId);
    printf("Start hooking process %d, thread id %d\n", pid, dwThreadId);
    printf("Press any key to quit\n");
    getchar();

    HookStop();

    return 0;
}


