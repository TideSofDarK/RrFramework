#include <Rr/Rr.h>

#include "../../Source/Rr_Arena.h"
#include <array>
#include <format>
#include <iostream>
#include <thread>
#include <vector>

#define THREAD_COUNT 16

constexpr std::array AllocSizes = {
    7, 15, 4, 69, 256, 389, 523, 876,
};

void ThreadOldLocked(Rr_Arena *Arena, Rr_Spinlock *Lock)
{
    for (auto Index = 0; Index < 5000; ++Index)
    {
        Rr_LockSpinlock(Lock);
        Rr_Alloc(AllocSizes[Index % AllocSizes.size()], Arena);
        Rr_UnlockSpinlock(Lock);
    }
}

void BenchmarkOldLocked(Rr_Arena *Arena)
{
    Rr_Spinlock Lock{};
    std::vector<std::jthread> Threads;
    Threads.reserve(THREAD_COUNT);
    for (auto Index = 0; Index < THREAD_COUNT; ++Index)
    {
        Threads.emplace_back(ThreadOldLocked, Arena, &Lock);
    }
}

void ThreadNew(Rr_TSArena *Arena)
{
    for (auto Index = 0; Index < 5000; ++Index)
    {
        Rr_TSAlloc(AllocSizes[Index % AllocSizes.size()], Arena);
    }
}

void BenchmarkNew(Rr_TSArena *Arena)
{
    std::vector<std::jthread> Threads;
    Threads.reserve(THREAD_COUNT);
    for (auto Index = 0; Index < THREAD_COUNT; ++Index)
    {
        Threads.emplace_back(ThreadNew, Arena);
    }
}

void Benchmark(size_t CommitSize)
{
    size_t ArenaTimeTotal = 0;
    size_t ArenaPositionTotal = 0;
    size_t TSArenaTimeTotal = 0;
    size_t TSArenaPositionTotal = 0;

    size_t Loops = 100;

    for (auto Index = 0; Index < Loops; ++Index)
    {
        {
            Rr_Arena *Arena = Rr_CreateArena(RR_MEGABYTES(1024), CommitSize);
            auto Now = Rr_GetPerformanceCounter();
            BenchmarkOldLocked(Arena);
            auto Time = Rr_GetPerformanceCounter() - Now;
            ArenaTimeTotal += Time;
            ArenaPositionTotal += Arena->Position;
            Rr_DestroyArena(Arena);
        }

        {
            Rr_TSArena *Arena =
                Rr_CreateTSArena(RR_MEGABYTES(1024), CommitSize);
            auto Now = Rr_GetPerformanceCounter();
            BenchmarkNew(Arena);
            auto Time = Rr_GetPerformanceCounter() - Now;
            TSArenaTimeTotal += Time;
            TSArenaPositionTotal += Rr_LoadAtomicIntRelaxed(&Arena->Position);
            Rr_DestroyTSArena(Arena);
        }
    }

    std::cout << std::format(
                     "==================COMMIT SIZE:{}KiB\n",
                     CommitSize)
                     .c_str();
    std::cout
        << std::format("Rr_Arena TIME: {}\n", ArenaTimeTotal / Loops).c_str();
    std::cout << std::format(
                     "Rr_Arena POSITION: {}\n",
                     ArenaPositionTotal / Loops)
                     .c_str();
    std::cout << std::format("Rr_TSArena TIME: {}\n", TSArenaTimeTotal / Loops)
                     .c_str();
    std::cout << std::format(
                     "Rr_TSArena POSITION: {}\n",
                     TSArenaPositionTotal / Loops)
                     .c_str();
}

int main()
{
    Rr_InitSystem();

    Benchmark(64);
    Benchmark(128);
    Benchmark(256);
    Benchmark(512);
    Benchmark(1024);
}
