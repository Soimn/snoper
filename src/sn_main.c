#include "sn_platform.h"

bool
Game_Init(Platform_State* platform_state)
{
    Platform = platform_state;
    
    return true;
}

void
Game_Tick(Platform_State* platform_state, Platform_Input platform_input)
{
    Platform = platform_state;
}