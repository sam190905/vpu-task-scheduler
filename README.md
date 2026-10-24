# VPU Priority Task Scheduler

A high-throughput, multi-threaded task scheduler written in modern **C++20**. Modeled after GPU/Tegra Kernel Mode Driver (KMD) work-submission engines, it schedules compute workloads under strict priority ordering with deterministic FIFO tie-breaking.

## Features

- **4-level priority system** — `REALTIME > HIGH > NORMAL > LOW`
- **Deterministic FIFO ordering** — tasks at the same priority execute in submission order
- **Thread-safe** — mutex + condition variable with predicate-based waiting (no spurious/lost wakeups)
- **Lock-free execution** — tasks execute outside the mutex to avoid priority inversion
- **Clean shutdown** — drains all remaining tasks before joining worker threads

## Project Structure

```
vpu_task_scheduler/
├── CMakeLists.txt              # Build configuration (C++20)
├── include/
│   └── TaskScheduler.hpp       # Header-only scheduler (~80 lines)
├── src/
│   └── main.cpp                # Demo with 3 test scenarios
├── run.bat                     # One-click build & run (Windows)
└── .gitignore
```

## How It Works

```
┌──────────────┐     ┌──────────────────────────┐     ┌─────────────┐
│  Producer(s)  │────▶│  Priority Queue (max-heap) │────▶│  Worker 1   │
│  submit()     │     │  sorted by:                │     │  Worker 2   │
│               │     │   1. priority (desc)       │     │  Worker 3   │
│               │     │   2. sequence ID (asc/FIFO)│     │  Worker N   │
└──────────────┘     └──────────────────────────┘     └─────────────┘
```

Workers **lock the mutex only to pop** from the queue, then **execute the task without holding the lock** — this is critical for throughput.

## Build & Run

### Using CMake

```bash
cmake -S . -B build
cmake --build build
./build/scheduler        # Linux/macOS
.\build\scheduler.exe    # Windows
```

### Direct compilation (no CMake)

```bash
g++ -std=c++20 -pthread -Iinclude src/main.cpp -o scheduler
./scheduler
```

### Windows one-click

```
run.bat
```

## Sample Output

```
=== VPU Priority Task Scheduler Demo ===

  [REALTIME] VSync display scanout
  [HIGH]     Physics simulation step
  [HIGH]     Audio buffer refill
  [NORMAL]   Texture decompression
  [LOW]      Telemetry upload
  [LOW]      Log flush to disk

--- FIFO test (all NORMAL priority) ---
  Task #1
  Task #2
  Task #3
  Task #4
  Task #5

--- Priority ordering test ---
  REALTIME
  HIGH
  NORMAL
  LOW

=== All demos completed. ===
```

## Key Design Decisions

| Question | Answer |
|---|---|
| **Why lock only around queue pop?** | Holding the mutex during task execution would serialize all threads — defeating the purpose of a thread pool. |
| **Why does `cv.wait()` use a predicate?** | Prevents spurious wakeups (OS can wake threads randomly) and lost wakeups (notify before wait). |
| **How does `operator<` work?** | `std::priority_queue` is a max-heap. Lower priority sinks down; for equal priority, higher sequence ID sinks (preserving FIFO). |

## Usage Example

```cpp
#include "TaskScheduler.hpp"

int main() {
    TaskScheduler scheduler(4);  // 4 worker threads

    scheduler.submit(Priority::REALTIME, [] {
        // Critical real-time work
    });

    scheduler.submit(Priority::LOW, [] {
        // Background cleanup
    });

    // Destructor automatically shuts down and drains the queue
}
```

## Requirements

- C++20 compiler (GCC 10+, Clang 12+, MSVC 19.29+)
- CMake 3.20+ (optional)
