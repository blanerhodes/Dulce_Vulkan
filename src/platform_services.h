#pragma once
//NOTE: this file is just forward declaring services that the platform provides to the engine

struct DebugReadFileResult {
    u32 contents_size;
    void* contents;
};

static DebugReadFileResult DebugPlatformReadEntireFile(char* filename);

static b32 DebugPlatformWriteEntireFile(char* filename, u32 memory_size, void* memory);

static void DebugPlatformFreeFileMemory(void* memory);
