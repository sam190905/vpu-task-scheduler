// main.cpp — Simple demo of the VPU Priority Task Scheduler
//
// Build:  g++ -std=c++20 -pthread -I../include main.cpp -o scheduler
// Run:    ./scheduler

#include "TaskScheduler.hpp"
#include <iostream>
#include <chrono>
#include <string>

int main() {
    std::cout << "=== VPU Priority Task Scheduler Demo ===\n\n";

    // ── 1. Create a scheduler with 4 worker threads ─────────────────
    TaskScheduler scheduler(4);

    // ── 2. Submit tasks at different priority levels ────────────────
    //    Each task just prints its name so we can see execution order.
    //    Higher-priority tasks will generally print before lower ones.

    scheduler.submit(Priority::LOW, [] {
        std::cout << "  [LOW]      Telemetry upload\n";
    });

    scheduler.submit(Priority::NORMAL, [] {
        std::cout << "  [NORMAL]   Texture decompression\n";
    });

    scheduler.submit(Priority::HIGH, [] {
        std::cout << "  [HIGH]     Physics simulation step\n";
    });

    scheduler.submit(Priority::REALTIME, [] {
        std::cout << "  [REALTIME] VSync display scanout\n";
    });

    scheduler.submit(Priority::HIGH, [] {
        std::cout << "  [HIGH]     Audio buffer refill\n";
    });

    scheduler.submit(Priority::LOW, [] {
        std::cout << "  [LOW]      Log flush to disk\n";
    });

    // Small pause to let the tasks above finish printing.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ── 3. Demonstrate deterministic FIFO within the same priority ──
    std::cout << "\n--- FIFO test (all NORMAL priority) ---\n";

    // We use a "gate" pattern: the first task blocks the single
    // worker while we enqueue the rest, so they all sit in the
    // queue and get drained in strict sequence-ID (FIFO) order.
    {
        TaskScheduler single(1);       // one worker → deterministic order

        std::mutex              gate_mtx;
        std::condition_variable gate_cv;
        bool                    gate_open = false;

        // Gate task — blocks the worker.
        single.submit(Priority::NORMAL, [&] {
            std::unique_lock<std::mutex> lk(gate_mtx);
            gate_cv.wait(lk, [&] { return gate_open; });
        });

        // These pile up behind the gate.
        for (int i = 1; i <= 5; ++i) {
            single.submit(Priority::NORMAL, [i] {
                std::cout << "  Task #" << i << "\n";
            });
        }

        // Release the gate — worker drains in FIFO order.
        {
            std::lock_guard<std::mutex> lk(gate_mtx);
            gate_open = true;
        }
        gate_cv.notify_one();

        // Destructor calls shutdown() → drains queue → joins worker.
    }

    // ── 4. Demonstrate strict priority ordering ─────────────────────
    std::cout << "\n--- Priority ordering test ---\n";
    {
        TaskScheduler single(1);

        std::mutex              gate_mtx;
        std::condition_variable gate_cv;
        bool                    gate_open = false;

        single.submit(Priority::REALTIME, [&] {
            std::unique_lock<std::mutex> lk(gate_mtx);
            gate_cv.wait(lk, [&] { return gate_open; });
        });

        // Submit in LOW→HIGH order; they should execute HIGH→LOW.
        single.submit(Priority::LOW,      [] { std::cout << "  LOW\n"; });
        single.submit(Priority::NORMAL,   [] { std::cout << "  NORMAL\n"; });
        single.submit(Priority::HIGH,     [] { std::cout << "  HIGH\n"; });
        single.submit(Priority::REALTIME, [] { std::cout << "  REALTIME\n"; });

        {
            std::lock_guard<std::mutex> lk(gate_mtx);
            gate_open = true;
        }
        gate_cv.notify_one();
    }

    std::cout << "\n=== All demos completed. ===\n";
    return 0;
}
