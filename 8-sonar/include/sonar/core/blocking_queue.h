#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

namespace sonar::core {

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : capacity_(capacity) {}

    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.size() >= capacity_) {
                queue_.pop_front();
                ++dropped_;
            }
            queue_.push_back(std::move(item));
        }
        cv_.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return stopped_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

    std::size_t dropped() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_;
    }

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<T> queue_;
    bool stopped_ {false};
    std::size_t dropped_ {0};
};

}  // namespace sonar::core
