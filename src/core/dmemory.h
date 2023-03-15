#pragma once
#include <string.h>

struct MemoryArena {
    u64 size;
    void* base;
    void* offset;
};
