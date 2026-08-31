#ifndef SPSCQUEUE_H
#define SPSCQUEUE_H

#include <vector>
#include <atomic>
#include <algorithm>
#include <cstddef>

template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity)
        : mCapacity(capacity + 1), mHead(0), mTail(0) {
        mBuffer.resize(mCapacity);
    }

    size_t getReadAvailable() const noexcept {
        size_t head = mHead.load(std::memory_order_acquire);
        size_t tail = mTail.load(std::memory_order_relaxed);
        if (head >= tail) {
            return head - tail;
        }
        return mCapacity - (tail - head);
    }

    size_t Available() const noexcept {
        return getReadAvailable();
    }

    bool Push(const T* data, size_t count) {
        size_t tail = mTail.load(std::memory_order_relaxed);
        size_t head = mHead.load(std::memory_order_acquire);

        size_t freeSpace = (head > tail) ? (head - tail - 1) : (mCapacity - 1 - (tail - head));
        if (count > freeSpace) return false;

        for (size_t i = 0; i < count; ++i) {
            mBuffer[(tail + i) % mCapacity] = data[i];
        }

        mTail.store((tail + count) % mCapacity, std::memory_order_release);
        return true;
    }

    bool Pop(T* destination, size_t count) {
        size_t head = mHead.load(std::memory_order_relaxed);
        size_t available = getReadAvailable();

        if (count > available) return false;

        for (size_t i = 0; i < count; ++i) {
            destination[i] = mBuffer[(head + i) % mCapacity];
        }

        mHead.store((head + count) % mCapacity, std::memory_order_release);
        return true;
    }

    void Clear() noexcept {
        mHead.store(0, std::memory_order_relaxed);
        mTail.store(0, std::memory_order_relaxed);
    }

private:
    std::vector<T> mBuffer;
    size_t mCapacity;
    std::atomic<size_t> mHead;
    std::atomic<size_t> mTail;
};

#endif // SPSCQUEUE_H