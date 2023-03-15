#include "dmemory.h"

//TODO: make unchecked version
static void ArenaPushBytes(MemoryArena* arena, void* data, u64 size) {
    if (!arena || !arena->base) {
        DERROR("ArenaPushBytes - Null arena passed");
        return;
    }
    if (!data) {
        DWARN("ArenaPushBytes - Null data passed. No bytes pushed.");
        return;
    }
    if ((u8*)arena->offset + size > (u8*)arena->base + arena->size) {
        DERROR("ArenaPushBytes - Attempted to allocate %d bytes but only %d bytes available.", size, (u8*)arena->base + arena->size - (u8*)arena->offset);
        return;
    }
    memcpy(arena->offset, data, size);
    arena->offset = (u8*)arena->offset + size;
}

//reserve space within an arena and returns a pointer to the start of the reserved block
static void* ArenaReserveBytes(MemoryArena* arena, u64 size) {
    if (!arena || !arena->base) {
        DERROR("ArenaPushBytes - Null arena passed");
        return 0;
    }
    if ((u8*)arena->offset + size > (u8*)arena->base + arena->size) {
        DERROR("ArenaPushBytes - Attempted to allocate %d bytes but only %d bytes available.", size, (u8*)arena->base + arena->size - (u8*)arena->offset);
        return 0;
    }
    //TODO: maybe 0 out memory since im going off the assumption that VirtualAlloc zeroes the memory on commit
    //      which could have issues if MEM_RESET is used
    void* result = arena->offset;
    arena->offset = (u8*)arena->offset + size;
    return result;
}

//TODO: check the pros/cons of zeroing the arena as opposed to reseting the offset to the beginning
//      and overwriting what is already there (write over junk vs write over zeroes)
//NOTE: keep an eye out for where I'm making assumptions that the memory is initialized to 0
static void ArenaZeroMemory(MemoryArena* arena) {
    //NOTE: sizeof(u8) is probably redundant, check this at some point
    memset(arena->base, 0, sizeof(u8) * arena->size);
}