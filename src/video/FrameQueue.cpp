#include "FrameQueue.h"

namespace PixelMotion {

FrameQueue::FrameQueue() {
}

FrameQueue::~FrameQueue() {
    // TODO: Clean up remaining frames
}

void FrameQueue::Push(void* frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(frame);
}

void* FrameQueue::Pop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_queue.empty()) {
        return nullptr;
    }

    void* frame = m_queue.front();
    m_queue.pop();
    return frame;
}

bool FrameQueue::IsEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();
}

size_t FrameQueue::Size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

} // namespace PixelMotion
