#pragma once
#include <string.h>

struct MemoryArena {
    u64 size;
    u8* base;
    u64 used;
};
