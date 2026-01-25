#pragma once

#include <queue>
#include <mutex>

namespace PixelMotion {

/**
 * Thread-safe frame queue
 * Producer-consumer pattern for decoded frames
 */
class FrameQueue {
public:
    FrameQueue();
    ~FrameQueue();

    void Push(void* frame);
    void* Pop();
    bool IsEmpty() const;
    size_t Size() const;

private:
    std::queue<void*> m_queue;
    mutable std::mutex m_mutex;
};

} // namespace PixelMotion
