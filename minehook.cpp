// dllmain.cpp : 定义 DLL 应用程序的入口点。
#pragma comment(lib, "shlwapi")

#include <Windows.h>
#include <Shlwapi.h>

// 用户定义
#define TARGET_PROCESS_NAME  L"MineSweeper.exe" // 待钩取的进程名
#define TARGET_WINDOW_NAME   L"扫雷"            // 目标进程主窗口名
#define ASM_LEN_RENDER       14 + 4 // render函数的注入代码长度，64位直接注入至少应当有14字节 + padding
#define RET_OFFSET_RENDER    13 // render函数的pop rax指令在code列表中的偏移，可能会随着code的变化而改变
#define ASM_LEN_MINES        14 + 1 // placemines函数的注入长度
#define RET_OFFSET_MINES     13 // placemines函数的pop rax指令在code列表中的偏移，可能会随着code的变化而改变
#define LINE_WIDTH 3 // 绘制方框的线宽

// 被注入代码信息
BYTE g_oldCode_render[ASM_LEN_RENDER] = { 0 }; // 保存被钩取的代码
BYTE g_oldCode_mines[ASM_LEN_MINES]   = { 0 };
DWORD64 g_attachAddr_render = NULL; // 保存注入的VA
DWORD64 g_attachAddr_mines = NULL;
DWORD64 g_retAddr_render = NULL; // 保存返回到被注入代码的地址
DWORD64 g_retAddr_mines = NULL;

// 句柄
HINSTANCE g_hModule = NULL;
HHOOK g_hHook = NULL;
HWND g_hWnd = NULL;

// 雷区信息
int g_boardHeight = 0;     // 盘面高度
int g_boardWidth  = 0;     // 盘面宽度
int g_mineCount   = 0;     // 地雷总数
int *g_mineBuffer = NULL;  // 地雷分布

// 其它
BOOL g_attachFlag = FALSE;
BOOL g_mineFlag   = FALSE; // 标记雷区信息是否已经获得
float g_base = 0;
float g_frame_offset = 0;
float g_tile_size = 0;



/* ------------------------------------------ */
// 获取雷区分布
void calcMines(int seed, int height, int width, int mineCount, int firstClickX, int firstClickY)
{
    int len = height * width;    
    int *tiles = NULL;
    tiles = (int*)malloc(len * sizeof(int));
    g_mineBuffer = (int*)malloc(mineCount * sizeof(int));

    int i = 0, j = 0;
    int count1 = 0;

    int flag1 = 0, flag2 = 0;
    int row = 0, column = 0;

    srand(seed);
    // init
    for(i=0; i<len; i++)
    {
        flag1 = 1;
        row = i / width - firstClickY;
        column = i % width - firstClickX;
        if(column < 0) flag1 = -1;
        if(column * flag1 <= 1)
        {
            flag2 = 1;
            if(row < 0) flag2 = -1;
            if(row * flag2 <= 1) continue;
        }
        tiles[count1++] = i;
    }

    //for(i=0;i<count1;i++) printf("0x%X ", tiles[i]);
    //printf("\n");

    int randnum = 0;
    for(i=0; i<mineCount; i++)
    {
        randnum = rand() % count1;
        g_mineBuffer[i] = tiles[randnum];
        // del index randnum
        for(j=randnum; j<count1; j++)
        {
            tiles[j] = tiles[j+1];
        }

        count1--;
    }

    g_mineFlag = TRUE;
    //for(i=0; i<mineCount; i++) printf("0x%X ", g_mineBuffer[i]);
}

void __declspec(naked) GetMines()
{
    // push all
    asm (
        ".intel_syntax noprefix\n"
        "push rsp\n"
        "push rax\n"
        "push rbx\n"
        "push rcx\n"
        "push rdx\n"
        "push rbp\n"
        "push rsi\n"
        "push rdi\n"
        "push r8\n"
        "push r9\n"
        "push r10\n"
        "push r11\n"
        "push r12\n"
        "push r13\n"
        "push r14\n"
        "push r15\n"
        "pushfq"
    );

    // 留出栈缓冲区，小心堆栈被毁
    asm (
        ".intel_syntax noprefix\n"
        "sub rsp, 0x80\n"
    );

    // 获取寄存器数据
    DWORD64 board = 0, firstClickX = 0, firstClickY = 0;
    asm (
        ".intel_syntax noprefix\n"
        "mov %0, rcx\n"
        "mov %1, rdx\n"
        "mov %2, r8\n"
        :"=a"(board), "=b"(firstClickX), "=d"(firstClickY)
    );
    int seed = *(int*)(board + 4 * 12);
    g_boardHeight = *(int*)(board + 4 * 3); 
    g_boardWidth  = *(int*)(board + 4 * 4);
    g_mineCount   = *(int*)(board + 4 * 2);
    // 计算雷区分布
    calcMines(seed, g_boardHeight, g_boardWidth, g_mineCount, (int)firstClickX, (int)firstClickY);

    // 恢复栈
    asm (
        ".intel_syntax noprefix\n"
        "add rsp, 0x80\n"
    );

    // pop all
    asm (
        ".intel_syntax noprefix\n"
        "popfq\n"
        "pop r15\n"
        "pop r14\n"
        "pop r13\n"
        "pop r12\n"
        "pop r11\n"
        "pop r10\n"
        "pop r9\n"
        "pop r8\n"
        "pop rdi\n"
        "pop rsi\n"
        "pop rbp\n"
        "pop rdx\n"
        "pop rcx\n"
        "pop rbx\n"
        "pop rax\n"
        "pop rsp\n"
    );

    // original code
    asm (
        ".intel_syntax noprefix\n"
        "mov qword ptr [rsp+0x10], rbx\n" // 地址偏移随堆栈调整
        "mov qword ptr [rsp+0x18], rbp\n"
        "mov qword ptr [rsp+0x20], rsi\n"
    );

    // return to target address
    // 分为两段，因为传参会占用一个寄存器，需要首先保护
    asm (
        ".intel_syntax noprefix\n"
        "push rax\n"
    );
    asm (
        ".intel_syntax noprefix\n"        
        "mov [rsp-0x8], %0\n"
        "pop rax\n"
        "jmp [rsp-0x10]"
        :             // output
        :"a"(g_retAddr_mines) // input
    );
}



/* ------------------------------------------ */
// 绘图函数
void setRect(RECT *r, int x, int y, int offset)
{
    r -> left   = g_frame_offset + x * g_tile_size + offset;
    r -> right  = g_frame_offset + (x + 1) * g_tile_size - offset;
    r -> top    = g_frame_offset + y * g_tile_size + offset;
    r -> bottom = g_frame_offset + (y + 1) * g_tile_size - offset;
}

void drawTile(HDC hdc, int x, int y, int lineWidth, HPEN pen)
{
    RECT rect;
    int i = 0;
    
    for(i = 0; i < lineWidth; i++)
    {
        setRect(&rect, x, y, i);
        FrameRect(hdc, &rect, (HBRUSH)pen);
    }
}

// 绘制方框
void InnerDrawGraph()
{
    // draw user defined graphics
    //PAINTSTRUCT ps;
    if(g_mineFlag)
    {
        HWND hCWnd = GetWindow(g_hWnd, GW_CHILD); // **获取扫雷程序的画布窗口，并非所有程序都要如此做 TODO : 子窗口的获取还是有问题
        HDC hdc = GetDC(hCWnd); // get hdc
        // 设置画笔
        HPEN mypen = CreatePen(PS_SOLID, 1, RGB(0xFF, 0x61, 0x3)); // color orange
        HPEN oldpen = (HPEN)SelectObject(hdc, mypen);

        // start drawing
        RECT window;
        GetWindowRect(hCWnd, &window); // 获取窗口尺寸
        int i = 0, row = 0, column = 0;
        g_base = (window.right - window.left)/(1.65*2 + g_boardWidth);
        g_tile_size = (int)(g_base + 0.5);
        g_frame_offset = (int)(1.65*g_base + 0.5);

        for(i = 0; i < g_mineCount; i++)
        {
            row = g_mineBuffer[i] / g_boardWidth;
            column = g_mineBuffer[i] % g_boardWidth;
            drawTile(hdc, column, row, LINE_WIDTH, mypen);
        }

        // end drwaing
        SelectObject(hdc, oldpen); // 还原旧的画笔
        DeleteObject(mypen);
        ReleaseDC(hCWnd, hdc);
    }
   
}

// 跳转代码，绘制雷区
void __declspec(naked) DrawMineGraph()
{
    // push all
    asm (
        ".intel_syntax noprefix\n"
        "push rsp\n"
        "push rax\n"
        "push rbx\n"
        "push rcx\n"
        "push rdx\n"
        "push rbp\n"
        "push rsi\n"
        "push rdi\n"
        "push r8\n"
        "push r9\n"
        "push r10\n"
        "push r11\n"
        "push r12\n"
        "push r13\n"
        "push r14\n"
        "push r15\n"
        "pushfq"
    );

    // 留出栈缓冲区，小心堆栈被毁
    asm (
        ".intel_syntax noprefix\n"
        "sub rsp, 0x80\n"
    );

    InnerDrawGraph();

    // 恢复栈
    asm (
        ".intel_syntax noprefix\n"
        "add rsp, 0x80\n"
    );

    // pop all
    asm (
        ".intel_syntax noprefix\n"
        "popfq\n"
        "pop r15\n"
        "pop r14\n"
        "pop r13\n"
        "pop r12\n"
        "pop r11\n"
        "pop r10\n"
        "pop r9\n"
        "pop r8\n"
        "pop rdi\n"
        "pop rsi\n"
        "pop rbp\n"
        "pop rdx\n"
        "pop rcx\n"
        "pop rbx\n"
        "pop rax\n"
        "pop rsp\n"
    );

    // original code
    asm (
        ".intel_syntax noprefix\n"
        "lea r11, qword ptr[rsp+0x218]\n" // 地址偏移随堆栈调整
        "movaps xmm6, xmmword ptr[r11-0x10]\n"
        "movaps xmm7, xmmword ptr[r11-0x20]\n"
    );

    // return to target address
    // 分为两段，因为传参会占用一个寄存器，需要首先保护
    asm (
        ".intel_syntax noprefix\n"
        "push rax\n"
    );
    asm (
        ".intel_syntax noprefix\n"        
        "mov [rsp-0x8], %0\n"
        "pop rax\n"
        "jmp [rsp-0x10]"
        :             // output
        :"a"(g_retAddr_render) // input
    );
}

// 注入代码
// 注入两个位置：1.布雷函数 2.渲染函数
// 注入采用push rax, jmp rax的形式实现（64位下直接jmp DWORD只能达到32位长度）
void AttachMines()
{
    DWORD64 rvaInjectAddr = 0x27614;                        // 注入地址
    BYTE code[ASM_LEN_MINES] = {
        0x50,                                               // push rax
        0x48, 0xB8,                                         // mov rax, addr
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // VA（后续通过memcpy填写，注意不是RVA，因为是jmp rax跳转）
        0xFF, 0xE0,                                         // jmp rax
        0x58,                                               // pop rax，此地址为执行结束后的返回地址，还原rax
        0x90,                             // padding
    };

    DWORD64 dwExeBase = (DWORD64)GetModuleHandleW(TARGET_PROCESS_NAME);
    if (!dwExeBase)
    {
        MessageBoxW(NULL, L"Failed to get minesweeper image base!", NULL, NULL);
        return;
    }

    DWORD64 targetFuncAddr = (DWORD64)GetMines;
    memcpy(code + 3, &targetFuncAddr, 8); // 将跳转地址写入

    //memcpy((void*)(dwExeBase + rvaInjectAddr), code, ASM_LEN_MINES); // 将代码注入到目标位置
    g_attachAddr_mines = dwExeBase + rvaInjectAddr;
    DWORD oldProtect = NULL;
    if (VirtualProtect((LPVOID)g_attachAddr_mines, ASM_LEN_MINES, PAGE_EXECUTE_READWRITE, &oldProtect)) // 一定要记得修改内存读写属性
    {
        ReadProcessMemory(GetCurrentProcess(), (LPVOID)g_attachAddr_mines, g_oldCode_mines, ASM_LEN_MINES, NULL); // 保存被钩取的代码
        WriteProcessMemory(GetCurrentProcess(), (LPVOID)g_attachAddr_mines, code, ASM_LEN_MINES, NULL);
        VirtualProtect((LPVOID)g_attachAddr_mines, ASM_LEN_MINES, oldProtect, &oldProtect);
    }

    g_retAddr_mines = g_attachAddr_mines + RET_OFFSET_MINES;
}

void AttachRender()
{
    DWORD64 rvaInjectAddr = 0x3E8F9;                        // 注入地址
    BYTE code[ASM_LEN_RENDER] = {
        0x50,                                               // push rax
        0x48, 0xB8,                                         // mov rax, addr
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // VA（后续通过memcpy填写，注意不是RVA，因为是jmp rax跳转）
        0xFF, 0xE0,                                         // jmp rax
        0x58,                                               // pop rax，此地址为执行结束后的返回地址，还原rax
        0x90, 0x90, 0x90, 0x90,                             // padding
    };

    DWORD64 dwExeBase = (DWORD64)GetModuleHandleW(TARGET_PROCESS_NAME);
    if (!dwExeBase)
    {
        MessageBoxW(NULL, L"Failed to get minesweeper image base!", NULL, NULL);
        return;
    }

    DWORD64 targetFuncAddr = (DWORD64)DrawMineGraph;
    memcpy(code + 3, &targetFuncAddr, 8); // 将跳转地址写入

    //memcpy((void*)(dwExeBase + rvaInjectAddr), code, ASM_LEN_RENDER); // 将代码注入到目标位置
    g_attachAddr_render = dwExeBase + rvaInjectAddr;
    DWORD oldProtect = NULL;
    if (VirtualProtect((LPVOID)g_attachAddr_render, ASM_LEN_RENDER, PAGE_EXECUTE_READWRITE, &oldProtect)) // 一定要记得修改内存读写属性
    {
        ReadProcessMemory(GetCurrentProcess(), (LPVOID)g_attachAddr_render, g_oldCode_render, ASM_LEN_RENDER, NULL); // 保存被钩取的代码
        WriteProcessMemory(GetCurrentProcess(), (LPVOID)g_attachAddr_render, code, ASM_LEN_RENDER, NULL);
        VirtualProtect((LPVOID)g_attachAddr_render, ASM_LEN_RENDER, oldProtect, &oldProtect);
    }

    g_retAddr_render = g_attachAddr_render + RET_OFFSET_RENDER;
}

void AttachCode()
{
    AttachMines();
    AttachRender();
}

void DetachCode()
{
    if(!g_attachAddr_render)
    {
        MessageBoxW(NULL, L"Render attach address is NULL", NULL, NULL);
        return;
    }
    if(!g_attachAddr_mines)
    {
        MessageBoxW(NULL, L"Placemines attach address is NULL", NULL, NULL);
        return;
    }
    // unhook render function
    DWORD oldProtect = NULL;
    if (VirtualProtect((LPVOID)g_attachAddr_render, ASM_LEN_RENDER, PAGE_EXECUTE_READWRITE, &oldProtect)) // 一定要记得修改内存读写属性
    {
        WriteProcessMemory(GetCurrentProcess(), (LPVOID)g_attachAddr_render, g_oldCode_render, ASM_LEN_RENDER, NULL);
        VirtualProtect((LPVOID)g_attachAddr_render, ASM_LEN_RENDER, oldProtect, &oldProtect);
    }
    // unhook placemines function
    oldProtect = NULL;
    if (VirtualProtect((LPVOID)g_attachAddr_mines, ASM_LEN_MINES, PAGE_EXECUTE_READWRITE, &oldProtect)) // 一定要记得修改内存读写属性
    {
        WriteProcessMemory(GetCurrentProcess(), (LPVOID)g_attachAddr_mines, g_oldCode_mines, ASM_LEN_MINES, NULL);
        VirtualProtect((LPVOID)g_attachAddr_mines, ASM_LEN_MINES, oldProtect, &oldProtect);
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        g_hModule = hModule;
        // 只有当进程为MineSweeper.exe时才进行代码注入
        DWORD dwpid = GetCurrentProcessId();
        WCHAR name[_MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, name, _MAX_PATH);
        PCWSTR subname = StrRChrW(name, NULL, L'\\');
        if (!lstrcmpW(subname + 1, TARGET_PROCESS_NAME))
        {
            //MessageBoxW(NULL, L"get it", NULL, NULL); // test code
            // 获取窗口句柄
            g_hWnd = FindWindowW(NULL, TARGET_WINDOW_NAME);
            if(!g_hWnd)
            {
                MessageBoxW(NULL, L"未找到目标窗口", NULL, NULL);
                break;
            }
            // 钩取目标代码
            AttachCode();
            g_attachFlag = TRUE;
        }
        break;
    }
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        // 如果已经进行过代码注入，则还原注入点
        if (g_attachFlag)
        {
            DetachCode();
            g_attachFlag = FALSE;
        }
        break;
    }
    return TRUE;
}

LRESULT CALLBACK HookMessage(int nCode, WPARAM wParam, LPARAM lParam)
{
    /*
    CWPRETSTRUCT* retStruct = (CWPRETSTRUCT*)lParam;

    if (retStruct->message == WM_PAINT) // hook WM_PAINT message
    {
        // draw user defined graphics
        GetMines();
        innerDrawGraph();
    }
    */

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// 消息钩取，钩取窗口消息
// 消息钩子可以改成直接远程注入的，这里是懒了不想改
#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) void HookStart(DWORD dwThreadID)
{
    g_hHook = SetWindowsHookExW(WH_CALLWNDPROC, HookMessage, g_hModule, dwThreadID);
    if (!g_hHook)
    {
        WCHAR buf[30] = { 0 };
        wsprintfW(buf, L"set hook error : 0x%X", GetLastError());
        MessageBoxW(NULL, buf, L"Error", MB_OK);
    }
}

__declspec(dllexport) void HookStop()
{
    if (g_hHook)
    {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
    }
}

#ifdef __cplusplus
}
#endif