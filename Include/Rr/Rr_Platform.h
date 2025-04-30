#pragma once

#include <Rr/Rr_Math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_PlatformInfo Rr_PlatformInfo;
struct Rr_PlatformInfo
{
    int PageSize;
    int AllocationGranularity;
};

extern bool Rr_InitPlatform(void);

extern Rr_PlatformInfo *Rr_GetPlatformInfo(void);

extern void *Rr_ReserveMemory(size_t Size);

extern void Rr_ReleaseMemory(void *Data, size_t Size);

extern bool Rr_CommitMemory(void *Data, size_t Size);

extern void Rr_DecommitMemory(void *Data, size_t Size);

typedef struct Rr_AtomicInt Rr_AtomicInt;
struct Rr_AtomicInt
{
    int Value;
};

extern int Rr_GetAtomicInt(Rr_AtomicInt *AtomicInt);

extern int Rr_SetAtomicInt(Rr_AtomicInt *AtomicInt, int Value);

typedef int Rr_Spinlock;

extern bool Rr_TryLockSpinlock(Rr_Spinlock *SpinLock);

extern void Rr_LockSpinlock(Rr_Spinlock *SpinLock);

extern void Rr_UnlockSpinlock(Rr_Spinlock *SpinLock);

typedef enum Rr_EventType
{
    RR_EVENT_TYPE_MOUSE_MOTION,
    RR_EVENT_TYPE_MOUSE_WHEEL,
    RR_EVENT_TYPE_MOUSE_BUTTON_DOWN,
    RR_EVENT_TYPE_MOUSE_BUTTON_UP,
    RR_EVENT_TYPE_TEXT_INPUT,
    RR_EVENT_TYPE_KEY_DOWN,
    RR_EVENT_TYPE_KEY_UP,
    RR_EVENT_TYPE_DROP_FILE,
    RR_EVENT_TYPE_QUIT,
} Rr_EventType;

typedef struct Rr_Event Rr_Event;
struct Rr_Event
{
    Rr_EventType Type;
    union
    {
        struct
        {
            Rr_Vec2 Position;
            Rr_Vec2 Delta;
        } MouseMotion;
        struct
        {
            Rr_Vec2 Position;
            uint8_t Button;
            uint8_t Clicks;
        } MouseButton;
        struct
        {
            Rr_Vec2 Position;
            Rr_Vec2 Amount;
        } Wheel;
        struct
        {
            const char *Path;
        } DropFile;
    };
};

extern bool Rr_PollEvent(Rr_Event *Event);

#ifdef __cplusplus
}
#endif
