#pragma once

struct DebugReadFileResult {
    u32 contents_size;
    void* contents;
};

DebugReadFileResult DebugPlatformReadEntireFile(char* filename);
void DebugPlatformFreeFileMemory(void* memory);
b32 DebugPlatformWriteEntireFile(char* filename, u32 memory_size, void* memory);

struct GameButtonState {
    int half_transition_count;
    b32 ended_down;
};

enum GameControllerKeys {
    KEY_MOVE_FORWARD,
    KEY_MOVE_DOWN,
    KEY_MOVE_LEFT,
    KEY_MOVE_RIGHT,
    KEY_ACTION_0,
    KEY_ACTION_1,
    KEY_ACTION_2,
    KEY_ACTION_3,
    KEY_LEFT_ALTERNATE,
    KEY_RIGHT_ALTERNATE,
    KEY_BACK,
    KEY_START,

    KEY_TERMINATOR
};

char* game_controller_key_strings[12] = {
    "MOVE_FORWARD   ",
    "MOVE_DOWN      ",
    "MOVE_LEFT      ",
    "MOVE_RIGHT     ",
    "ACTION_0       ",
    "ACTION_1       ",
    "ACTION_2       ",
    "ACTION_3       ",
    "LEFT_ALTERNATE ",
    "RIGHT_ALTERNATE",
    "BACK           ",
    "START          "
};

struct GameControllerInput {
    b32 is_connected;
    //b32 is_analog;    
    
    union {
        GameButtonState buttons[12];
        struct {
            GameButtonState move_up;
            GameButtonState move_down;
            GameButtonState move_left;
            GameButtonState move_right;
            
            GameButtonState action_up;
            GameButtonState action_down;
            GameButtonState action_left;
            GameButtonState action_right;
            
            GameButtonState left_shoulder;
            GameButtonState right_shoulder;

            GameButtonState back;
            GameButtonState start;
        };
        struct {
            GameButtonState move_forward;
            GameButtonState move_backward;
            GameButtonState move_l;
            GameButtonState move_r;
            
            GameButtonState action_0;
            GameButtonState action_1;
            GameButtonState action_2;
            GameButtonState action_3;
            
            GameButtonState left_alternate;
            GameButtonState right_alternate;

            GameButtonState menu;
            GameButtonState pause;
        };
    };
    GameButtonState terminator;
};

struct GameInput {
    GameControllerInput controllers[5];
};

struct GameMemory {
    b32 is_initialized;
    u64 permanent_storage_size;
    void* permanent_storage;

    u64 transient_storage_size;
    void* transient_storage;
};

struct GameState {

};

static void GameUpdateAndRender(GameInput* input);