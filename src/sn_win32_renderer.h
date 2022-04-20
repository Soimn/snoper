TYPEDEF_FUNC(void, renderer_begin_frame);
TYPEDEF_FUNC(void, renderer_end_frame);

typedef struct Win32_Renderer_State
{
    renderer_begin_frame BeginFrame;
    renderer_end_frame EndFrame;
} Win32_Renderer_State;

internal bool
Win32_SetupRenderer(Win32_Renderer_State* renderer_state, HINSTANCE instance, HWND window_handle)
{
    bool succeeded = false;
    
    if (Win32_GLInit(instance, window_handle))
    {
        renderer_state->BeginFrame = &Win32_GLBeginFrame;
        renderer_state->EndFrame   = &Win32_GLEndFrame;
        
        succeeded = true;
    }
    
    return succeeded;
}