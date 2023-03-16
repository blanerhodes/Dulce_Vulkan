#include <windows.h>
#include <vulkan/vulkan.h>

#include "defines.h"
#include "asserts.h"
#include "core/logger.cpp"
#include "core/dmemory.cpp"
#include "math/dmath.cpp"
#include "core/dstring.cpp"

#include "vulkan/vulkan_utils.cpp"
#include "vulkan/vulkan_renderer.cpp"

#include "dulce.cpp"

b8 global_running = false;


static DebugReadFileResult DebugPlatformReadEntireFile(char* filename) {
    DebugReadFileResult result = {};
    HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);

    if (file_handle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_size;
        if (GetFileSizeEx(file_handle, &file_size)) {
            DASSERT(file_size.QuadPart <= 0xFFFFFFFF);
            result.contents = VirtualAlloc(0, file_size.QuadPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (result.contents) {
                DWORD bytes_read;
                if (ReadFile(file_handle, result.contents, file_size.QuadPart, &bytes_read, 0) && file_size.QuadPart == bytes_read) {
                    //read successfully
                    result.contents_size = file_size.QuadPart;
                }
            } else {
                DebugPlatformFreeFileMemory(result.contents);
                result.contents = 0;
            }
        }
        CloseHandle(file_handle);
    }
    return result;
}

static void DebugPlatformFreeFileMemory(void* memory) {
    if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

static b32 DebugPlatformWriteEntireFile(char* filename, u32 memory_size, void* memory) {
    b32 result = false;
    HANDLE file_handle = CreateFileA(filename, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, NULL, NULL);

    if (file_handle != INVALID_HANDLE_VALUE) {
        DWORD bytes_written;
        if (WriteFile(file_handle, memory, memory_size, &bytes_written, NULL)) {
            //success
            result = bytes_written == memory_size;
        } else {
            //log error
        }
        CloseHandle(file_handle);
    } else {
        //log error
    }
    return result;
}

static void Win32ProcessKeyboardMessage(GameButtonState* new_state, b32 is_down) {
    DASSERT(new_state->ended_down != is_down);
    new_state->ended_down = is_down;
    ++new_state->half_transition_count;
}

static void Win32ProcessPendingMessages(GameControllerInput* keyboard_controller) {
    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch(message.message) {
            case WM_QUIT:{
                global_running = false;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:{
                u32 vk_code = (u32)message.wParam;
                b32 was_down = ((message.lParam & (1 << 30)) != 0);
                b32 is_down = ((message.lParam & (1 << 31)) == 0);
                if (was_down != is_down) {
                    if(vk_code == 'W') {
                        Win32ProcessKeyboardMessage(&keyboard_controller->move_up, is_down);
                    }
                    else if(vk_code == 'A')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->move_left, is_down);
                    }
                    else if(vk_code == 'S')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->move_down, is_down);
                    }
                    else if(vk_code == 'D')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->move_right, is_down);
                    }
                    else if(vk_code == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->left_shoulder, is_down);
                    }
                    else if(vk_code == 'E')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->right_shoulder, is_down);
                    }
                    else if(vk_code == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->action_up, is_down);
                    }
                    else if(vk_code == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->action_left, is_down);
                    }
                    else if(vk_code == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->action_down, is_down);
                    }
                    else if(vk_code == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->action_right, is_down);
                    }
                    else if(vk_code == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->start, is_down);
                    }
                    else if(vk_code == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->back, is_down);
                    }
                }

                b32 alt_key_was_down = (message.lParam & (1 << 29));
                if ((vk_code == VK_F4) && alt_key_was_down) {
                    global_running = false;
                }
            } break;

            default: {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            } break;
        }
    }
}

static LRESULT CALLBACK Win32MainWindowCallback(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {       
    LRESULT result = 0;
    switch(message) {
        case WM_CLOSE: {
            global_running = false;
        } break;
        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        case WM_DESTROY: {
            global_running = false;
        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            DASSERT(!"Keyboard input came in through a non-dispatch message!");
        } break;
        default: {
            result = DefWindowProcA(window, message, w_param, l_param);
        } break;
    }
    return result;
}

static void Win32ArenaAlloc(MemoryArena* arena, u64 size) {
    //TODO: check for page aligned allocations
    arena->size = size; 
    arena->used = 0;
}

int main(void) {
    HINSTANCE instance = GetModuleHandleA(0);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = Win32MainWindowCallback;
    wc.hInstance = instance;
    wc.lpszClassName = "VulkanWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassA(&wc)) {
        MessageBoxA(0, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }
    u32 window_x = 300;
    u32 window_y = 300;
    u32 window_width = 1280;
    u32 window_height = 720;

    u32 window_ex_style = WS_EX_APPWINDOW;
    u32 window_style = WS_SYSMENU | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME | WS_VISIBLE | WS_OVERLAPPEDWINDOW;
    HWND window = CreateWindowExA(window_ex_style, wc.lpszClassName, "Vulkan Learning", window_style,
                                  window_x, window_y, window_width, window_height,
                                  NULL, NULL, instance, NULL);
    if (!window) {
        MessageBoxA(0, "Failed to create window", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    Win32StateHandles state_handles = {};
    state_handles.hwnd = window;
    state_handles.hInstance = instance;
    u64 renderer_memory_requirement = MegaBytes(1);
    VulkanContext vulkan_context = {};
    vulkan_context.frame_buffer_width = window_width;
    vulkan_context.frame_buffer_height = window_height;
    vulkan_context.allocator = 0;
    InitializeArena(&vulkan_context.transient_memory, renderer_memory_requirement, 
        (u8*)VirtualAlloc(0, renderer_memory_requirement, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
    VulkanRendererInitialize(state_handles, &vulkan_context);

    global_running = true;

    GameInput input[2] = {};
    GameInput* new_input = &input[0];
    GameInput* old_input = &input[1];

    while (global_running) {

        //get input
        GameControllerInput* old_keyboard_controller = GetController(old_input, 0);
        GameControllerInput* new_keyboard_controller = GetController(new_input, 0);
        *new_keyboard_controller = {};
        new_keyboard_controller->is_connected = true;
        
        for (i32 button_index = 0; button_index < ArrayCount(new_keyboard_controller->buttons); button_index++) {
            new_keyboard_controller->buttons[button_index].ended_down = 
                old_keyboard_controller->buttons[button_index].ended_down;
        }

        //process input
        Win32ProcessPendingMessages(new_keyboard_controller);

        //game update and render call
        GameUpdateAndRender(new_input);

        //swap old and new input
        GameInput* temp = new_input;
        new_input = old_input;
        old_input = temp;
    }

    VulkanRendererShutdown(&vulkan_context);
    return true;
}