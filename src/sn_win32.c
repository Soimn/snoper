#define UNICODE
#define NOMINMAX            1
#define WIN32_LEAN_AND_MEAN 1
#define WIN32_MEAN_AND_LEAN 1
#define VC_EXTRALEAN        1
#include <windows.h>
#include <Timeapi.h>
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#undef WIN32_MEAN_AND_LEAN
#undef VC_EXTRALEAN
#undef far
#undef near

#include "sn_platform.h"
#include "sn_win32_renderer_gl.h"
#include "sn_win32_renderer.h"

typedef struct Win32_File_Timestamp
{
    u64 creation;
    u64 last_access;
    u64 last_write;
} Win32_File_Timestamp;

typedef struct Win32_Code
{
    Win32_File_Timestamp timestamp;
    HMODULE handle;
    game_init_func init;
    game_tick_func tick;
    bool is_odd;
} Win32_Code;

void*
Win32_ReserveMemory(umm size)
{
    return VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
}

bool
Win32_CommitMemory(void* address, umm size)
{
    return !!VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE);
}

void
Win32_FreeMemory(void* address)
{
    VirtualFree(address, 0, MEM_RELEASE);
}

internal void
Win32_ErrorBox(char* msg)
{
    MessageBoxA(0, msg, "Fatal Error", MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
}

internal void
Win32_Print(char* msg, ...)
{
    Buffer buffer = {
        .size = Arena_Space(Platform->frame_arena)
    };
    
    Arena_Marker marker = Arena_BeginTemp(Platform->frame_arena);
    
    buffer.data = Arena_PushSize(Platform->frame_arena, buffer.size, 1);
    
    va_list arg_list;
    va_start(arg_list, msg);
    String_FormatArgList(buffer, msg, arg_list);
    va_end(arg_list);
    
    OutputDebugStringA((LPCSTR)buffer.data);
    
    Arena_EndTemp(Platform->frame_arena, marker);
}

internal bool
Win32_GetFileTimestamp(LPWSTR path, Win32_File_Timestamp* timestamp)
{
    bool succeeded = false;
    
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &attributes))
    {
        timestamp->creation    = (u64)attributes.ftCreationTime.dwHighDateTime   << 32 | attributes.ftCreationTime.dwLowDateTime;
        timestamp->last_write  = (u64)attributes.ftLastWriteTime.dwHighDateTime  << 32 | attributes.ftLastWriteTime.dwLowDateTime;
        timestamp->last_access = (u64)attributes.ftLastAccessTime.dwHighDateTime << 32 | attributes.ftLastAccessTime.dwLowDateTime;
        
        succeeded = true;
    }
    
    return succeeded;
}

internal bool
Win32_TryLoadCode(Win32_Code* code, LPWSTR code_path, LPWSTR loaded_code_path_odd, LPWSTR loaded_code_path_even)
{
    bool succeeded = false;
    
    LPWSTR old_path = loaded_code_path_even;
    LPWSTR new_path = loaded_code_path_odd;
    if (code->is_odd)
    {
        LPWSTR tmp = old_path;
        old_path = new_path;
        new_path = tmp;
    }
    
    Win32_File_Timestamp new_timestamp;
    
    if (CopyFileW(code_path, new_path, 0) && Win32_GetFileTimestamp(code_path, &new_timestamp))
    {
        HMODULE handle = LoadLibraryW(new_path);
        if (handle != 0)
        {
            game_init_func init = (game_init_func)GetProcAddress(handle, "Game_Init");
            game_tick_func tick = (game_tick_func)GetProcAddress(handle, "Game_Tick");
            
            if (init != 0 && tick != 0)
            {
                if (code->handle != 0) FreeLibrary(code->handle);
                
                code->timestamp = new_timestamp;
                code->handle    = handle;
                code->init      = init;
                code->tick      = tick;
                code->is_odd    = !code->is_odd;
                
                succeeded = true;
            }
        }
    }
    
    return succeeded;
}

LRESULT CALLBACK
WindowProc(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;
    
    if (message == WM_QUIT || message == WM_CLOSE) PostQuitMessage(0);
    else result = DefWindowProc(window_handle, message, wparam, lparam);
    
    return result;
}

void __stdcall
WinMainCRTStartup()
{
    HINSTANCE instance = GetModuleHandle(0);
    Win32_Code code    = {0};
    HWND window_handle = 0;
    
    WNDCLASSW window_class = {
        .lpfnWndProc   = &WindowProc,
        .hInstance     = instance,
        .lpszClassName = L"Snoper",
    };
    
    if (!RegisterClassW(&window_class)) Win32_ErrorBox("Failed to register window class\n");
    else
    {
        window_handle = CreateWindowExW(0, L"Snoper", L"Snoper", WS_OVERLAPPEDWINDOW,
                                        CW_USEDEFAULT, CW_USEDEFAULT,
                                        CW_USEDEFAULT, CW_USEDEFAULT,
                                        0, 0, instance, 0);
        
        if (window_handle == INVALID_HANDLE_VALUE) Win32_ErrorBox("Failed to create window\n");
        else
        {
            bool setup_failed = false;
            
            /// Setup platform binding
            Platform_State platform_state = {
                .ReserveMemory = &Win32_ReserveMemory,
                .CommitMemory  = &Win32_CommitMemory,
                .FreeMemory    = &Win32_FreeMemory,
            };
            
            Platform = &platform_state;
            
            platform_state.persistent_arena = Arena_Init(GB(2));
            platform_state.frame_arena      = Arena_Init(GB(2));
            if (platform_state.persistent_arena == 0 || platform_state.frame_arena == 0)
            {
                Win32_ErrorBox("Failed to allocate necessary memory\n");
                setup_failed = true;
            }
            
            /// Setup paths
            LPWSTR code_path             = L"game_code.dll";
            LPWSTR loaded_code_path_odd  = L"game_code_loaded_odd.dll";
            LPWSTR loaded_code_path_even = L"game_code_loaded_even.dll";
            
            if (!setup_failed)
            {
                Arena_Marker marker = Arena_BeginTemp(platform_state.frame_arena);
                
                umm buffer_size = Arena_Space(platform_state.frame_arena);
                ASSERT(buffer_size <= U32_MAX);
                
                void* buffer = Arena_PushSize(platform_state.frame_arena, buffer_size, 1);
                
                u32 length = GetModuleFileNameW(instance, buffer, (u32)buffer_size);
                
                LPWSTR end = buffer;
                for (umm i = 0; i < length; ++i)
                {
                    if (((LPWSTR)buffer)[i] == '\\' ||
                        ((LPWSTR)buffer)[i] == '/')
                    {
                        end = (LPWSTR)buffer + i + 1;
                    }
                }
                *end = 0;
                
                if (length == 0 || length == buffer_size || !SetCurrentDirectoryW(buffer))
                {
                    Win32_ErrorBox("Failed to set up paths to game code\n");
                    setup_failed = true;
                }
                
                Arena_EndTemp(platform_state.frame_arena, marker);
            }
            
            /// Load code
            if (!setup_failed)
            {
                for (umm tries = 0;; ++tries)
                {
                    if (Win32_TryLoadCode(&code, code_path, loaded_code_path_odd, loaded_code_path_even))
                    {
                        if (code.init(&platform_state))
                        {
                            Win32_Print("Successfully loaded code\n");
                        }
                        else setup_failed = true;
                        
                        break;
                    }
                    else if (tries == 100)
                    {
                        Win32_ErrorBox("Failed to load game code\n");
                        setup_failed = true;
                        break;
                    }
                }
            }
            
            /// Setup renderer
            Win32_Renderer_State renderer_state = {0};
            if (!setup_failed)
            {
                if (!Win32_SetupRenderer(&renderer_state, instance, window_handle))
                {
                    setup_failed = true;
                }
            }
            
            if (!setup_failed)
            {
                ShowWindow(window_handle, SW_SHOW);
                
                // NOTE: request minimum resolution of 1 ms for timers, affects Sleep
                timeBeginPeriod(1);
                
                LARGE_INTEGER perf_freq;
                QueryPerformanceFrequency(&perf_freq);
                
                LARGE_INTEGER flip_time;
                QueryPerformanceCounter(&flip_time);
                
                bool running = true;
                while (running)
                {
                    /// Reload code if necessary
                    Win32_File_Timestamp code_timestamp;
                    if (Win32_GetFileTimestamp(code_path, &code_timestamp) &&
                        !MemMatch(&code_timestamp, &code.timestamp, sizeof(Win32_File_Timestamp)))
                    {
                        Win32_TryLoadCode(&code, code_path, loaded_code_path_odd, loaded_code_path_even);
                        Win32_Print("Successfully reloaded code\n");
                    }
                    
                    MSG msg;
                    while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
                    {
                        if (msg.message == WM_QUIT)
                        {
                            running = false;
                            break;
                        }
                        
                        else DispatchMessage(&msg);
                    }
                    
                    Platform_Input input = {0};
                    
                    renderer_state.BeginFrame();
                    code.tick(Platform, input);
                    renderer_state.EndFrame();
                    
                    /// End of frame cleanup
                    LARGE_INTEGER end_time;
                    QueryPerformanceCounter(&end_time);
                    
                    Win32_Print("frame time: %ums\n", (u32)(1000 * (f32)(end_time.QuadPart - flip_time.QuadPart) / perf_freq.QuadPart));
                    
                    flip_time = end_time;
                }
                
                timeEndPeriod(1);
                
                FreeLibrary(code.handle);
                DeleteFile(loaded_code_path_odd);
                DeleteFile(loaded_code_path_even);
            }
        }
    }
    
    ExitProcess(0);
}
