#include "dulce.h"
#include <stdio.h>

static char* GameControllerKeyIndexToString(GameControllerKeys key) {
    char* result = game_controller_key_strings[key];
    return result;
}

inline GameControllerInput* GetController(GameInput* input, u32 controller_index) {
    DASSERT(controller_index < ArrayCount(input->controllers));
    
    GameControllerInput* result = &input->controllers[controller_index];
    return result;
}

//needs to take keyboard input, sound buffer later on, timing, renderer hook?
static void GameUpdateAndRender(GameInput* input) {
    DASSERT((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) == (ArrayCount(input->controllers[0].buttons)));

    for (i32 controller_index = 0; controller_index < 1; controller_index++) {
        GameControllerInput* controller = GetController(input, controller_index);
        if (controller->move_forward.ended_down) {
        }
    }
}