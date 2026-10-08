#include "Tasks/Tasks.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    auto& Scheduler = Tasks::FTaskScheduler::Get();
    // Initialization is optional: Launch and pipes must still complete synchronously.
    assert(Tasks::Launch("InlineResult", [] { return 42; }).GetResult() == 42);
    {
        Tasks::FPipe Pipe;
        int Value = 0;
        Pipe.Launch([&] { ++Value; }).Wait();
        assert(Value == 1);
    }
    Scheduler.Initialize(4);
    std::vector<std::atomic<int>> Visits(10003);
    Tasks::ParallelForChunks(static_cast<uint32_t>(Visits.size()), 37,
        [&](uint32_t Begin, uint32_t End, uint32_t Chunk)
        {
            assert(Chunk < 37 && Begin < End);
            for (uint32_t Index = Begin; Index < End; ++Index) ++Visits[Index];
        });
    for (const auto& Visit : Visits) assert(Visit.load() == 1);

    std::atomic<int> NestedCount{0};
    Tasks::ParallelFor(20, 1, [&](int First, int Last)
    {
        for (int Outer = First; Outer < Last; ++Outer)
            Tasks::ParallelFor(31, 3, [&](int Begin, int End) { NestedCount += End - Begin; });
    });
    assert(NestedCount == 620);

    {
        Tasks::FPipe Pipe;
        int Expected = 0;
        for (int Index = 0; Index < 10000; ++Index)
            Pipe.Launch([&, Index] { assert(Expected == Index); ++Expected; });
        Pipe.WaitUntilEmpty();
        assert(Expected == 10000);
    }

    std::atomic<bool> Gate{false};
    std::atomic<int> Dependents{0};
    auto Prerequisite = Tasks::Launch("Prerequisite", [&]
    {
        while (!Gate.load(std::memory_order_acquire)) std::this_thread::yield();
    });
    // Intentionally discard every returned handle while the prerequisite is blocked.
    for (int Index = 0; Index < 2000; ++Index)
        Tasks::Launch("DiscardedDependent", [&] { ++Dependents; }, Prerequisite);
    Gate.store(true, std::memory_order_release);
    Prerequisite.Wait();

    std::atomic<int> RingCount{0};
    std::atomic<bool> RingStarted{false};
    auto RingParent = Tasks::Launch("RingReuse", [&]
    {
        RingStarted.store(true, std::memory_order_release);
        for (int Index = 0; Index < 50000; ++Index)
            Tasks::Launch("RingChild", [&] { ++RingCount; });
    });
    while (!RingStarted.load(std::memory_order_acquire)) std::this_thread::yield();
    RingParent.Wait();

    std::atomic<int> FinishedAfterException{0};
    bool Caught = false;
    try
    {
        Tasks::ParallelFor(20, 1, [&](int Begin, int)
        {
            if (Begin == 19) throw std::runtime_error("expected");
            ++FinishedAfterException;
        });
    }
    catch (const std::runtime_error&) { Caught = true; }
    assert(Caught && FinishedAfterException == 19);
    Caught = false;
    try { Tasks::Launch("WorkerException", [] { throw std::runtime_error("expected"); }).Wait(); }
    catch (const std::runtime_error&) { Caught = true; }
    assert(Caught);

    // Shutdown drains queued work and prerequisite chains without discarding callbacks.
    Scheduler.Shutdown();
    assert(Dependents == 2000 && RingCount == 50000);
    Scheduler.Initialize(2);
    assert(Tasks::Launch("RestartResult", [] { return 7; }).GetResult() == 7);
    Scheduler.Shutdown();
    std::cout << "PASS: chunks, nested ParallelFor, pipe ordering, discarded dependents, ring reuse, exceptions, shutdown and restart\n";
}
