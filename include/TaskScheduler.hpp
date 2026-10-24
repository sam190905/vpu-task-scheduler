#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <vector>

// ── Priority levels (higher value = higher priority) ────────────────────
enum class Priority { LOW = 0, NORMAL = 1, HIGH = 2, REALTIME = 3 };

// ── A single unit of work with scheduling metadata ──────────────────────
struct Task {
    Priority              priority;
    uint64_t              id;        // monotonic ID for FIFO tie-breaking
    std::function<void()> func;

    // std::priority_queue is a MAX-heap, so this comparator makes the
    // *highest* priority pop first.  When two tasks share the same
    // priority, the one with the *smaller* id (submitted earlier) pops
    // first — that's FIFO ordering.
    bool operator<(const Task& other) const {
        if (priority == other.priority)
            return id > other.id;   // smaller id wins → FIFO
        return static_cast<int>(priority) < static_cast<int>(other.priority);
    }
};

// ── The scheduler: a thread pool draining a shared priority queue ───────
class TaskScheduler {
public:
    explicit TaskScheduler(int thread_count) {
        for (int i = 0; i < thread_count; ++i)
            workers.emplace_back([this] { worker_loop(); });
    }

    // Submit a task: just pass a priority and a void() lambda.
    void submit(Priority p, std::function<void()> fn) {
        {
            // LOCK SCOPE: we hold the mutex only long enough to push
            // onto the queue.  Executing the task inside this lock
            // would block every other thread — so we never do that.
            std::lock_guard<std::mutex> lock(mtx);
            queue.push({p, next_id++, std::move(fn)});
        }
        // Wake one sleeping worker so it can pick up the new task.
        cv.notify_one();
    }

    // Signal workers to finish, drain remaining tasks, then join.
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();                       // wake every sleeping worker
        for (auto& w : workers)
            if (w.joinable()) w.join();
    }

    ~TaskScheduler() { shutdown(); }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mtx);

                // WHY the predicate?  condition_variable::wait() can
                // wake up spuriously (OS decides to).  The predicate
                // re-checks the actual condition so we never proceed
                // on a false wakeup.  It also catches the "stop" flag.
                cv.wait(lock, [this] { return stop || !queue.empty(); });

                // If we were woken because of shutdown AND the queue
                // is empty, there's nothing left to do — exit.
                if (stop && queue.empty()) return;

                task = std::move(const_cast<Task&>(queue.top()).func);
                queue.pop();
            }
            // EXECUTE OUTSIDE THE LOCK — this is critical:
            // holding the mutex during execution would serialize
            // all tasks and defeat the purpose of multiple threads.
            task();
        }
    }

    std::vector<std::thread>    workers;
    std::priority_queue<Task>   queue;
    std::mutex                  mtx;
    std::condition_variable     cv;
    bool                        stop    = false;
    uint64_t                    next_id = 0;
};
