#include "dmemory.h"

#define PushStruct(arena, type) (type *)PushSize_(arena, sizeof(type))
#define PushArray(arena, count, type) (type *)PushSize_(arena, (count) * sizeof(type))
#define PushSize(arena, size) (u8*)PushSize_(arena, size)

void* PushSize_(MemoryArena* arena, size_t size) {
    DASSERT((arena->used + size) <= arena->size);
    void* result = arena->base + arena->used;
    arena->used += size;

    return result;
}

static void InitializeArena(MemoryArena* arena, u64 size, u8* base) {
    arena->size = size;
    arena->base = base;
    arena->used = 0;
}

//TODO: check the pros/cons of zeroing the arena as opposed to reseting the used to the beginning
//      and overwriting whoffsets already there (write over junk vs write over zeroes)
static void ArenaZeroMemory(MemoryArena* arena) {
    memset(arena->base, 0, arena->size);
}
