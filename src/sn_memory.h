internal inline umm
RoundUp(umm n, umm alignment)
{
    return (n + (alignment - 1)) & ~alignment;
}

internal inline umm
RoundDown(umm n, umm alignment)
{
    return n & ~alignment;
}

internal inline void*
Align(void* ptr, u8 alignment)
{
    return (void*)RoundUp((umm)ptr, alignment);
}

internal inline u8
AlignOffset(void* ptr, u8 alignment)
{
    return (u8)((u8*)Align(ptr, alignment) - (u8*)ptr);
}

internal inline void
Zero(void* ptr, umm size)
{
    for (u8* bptr = ptr;
         bptr < (u8*)ptr + size;
         ++bptr)
    {
        *bptr = 0;
    }
}

#define ZeroStruct(S) Zero((S), sizeof(*(S)))

internal inline void
Copy(void* src, void* dst, umm size)
{
    for (umm i = 0; i < size; ++i)
    {
        ((u8*)dst)[i] = ((u8*)src)[i];
    }
}

internal inline void
Move(void* src, void* dst, umm size)
{
    u8* bsrc = src;
    u8* bdst = dst;
    
    if (bsrc <= bdst && bsrc + size > bdst)
    {
        for (u64 i = size - 1; i < size; --i)
        {
            ((u8*)dst)[i] = ((u8*)src)[i];
        }
    }
    else Copy(src, dst, size);
}

internal inline bool
MemMatch(void* a, void* b, umm size)
{
    bool result = true;
    
    for (umm i = 0; i < size; ++i)
    {
        if (((u8*)a)[i] != ((u8*)b)[i])
        {
            result = false;
            break;
        }
    }
    
    return result;
}

typedef struct Arena
{
    u32 offset;
    u32 space;
} Arena;

typedef  u32 Arena_Marker;

Arena*
Arena_Init(umm size)
{
    ASSERT(size <= U32_MAX);
    
    Arena* result = 0;
    
    umm alloc_size = sizeof(Arena) + size;
    void* memory = Platform->ReserveMemory(alloc_size);
    
    if (memory != 0 && Platform->CommitMemory(memory, alloc_size))
    {
        result  = memory;
        *result = (Arena){
            .offset = 0,
            .space  = (u32)size,
        };
    }
    
    return result;
}

void*
Arena_PushSize(Arena* arena, umm size, u8 alignment)
{
    ASSERT(size <= U32_MAX && alignment > 0 && ((alignment - 1) & alignment) == 0);
    
    u8* cursor     = (u8*)(arena + 1) + arena->offset;
    umm cursor_adj = AlignOffset(cursor, alignment);
    
    ASSERT(cursor_adj + size <= arena->space);
    
    void* result = cursor + cursor_adj;
    
    u32 advancement = (u32)(cursor_adj + size);
    arena->offset += advancement;
    arena->space  -= advancement;
    
    return result;
}

void
Arena_Clear(Arena* arena)
{
    arena->space += arena->offset;
    arena->offset = 0;
}

void
Arena_Free(Arena* arena)
{
    Platform->FreeMemory(arena);
}

Arena_Marker
Arena_BeginTemp(Arena* arena)
{
    return arena->offset;
}

void
Arena_EndTemp(Arena* arena, Arena_Marker marker)
{
    // NOTE: This won't catch all abuse (e.g. clearing the arena while using temp memory, and then pushing past the marker)
    //       but it should catch some
    ASSERT((imm)arena->offset - (imm)marker >= 0);
    
    arena->space += arena->offset - marker;
    arena->offset = marker;
}

umm
Arena_Offset(Arena* arena)
{
    return arena->offset;
}

umm
Arena_Space(Arena* arena)
{
    return arena->space;
}