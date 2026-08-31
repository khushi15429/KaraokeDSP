#ifndef AUDIOBUFFER_H
#define AUDIOBUFFER_H

#pragma warning(disable: 4244)
#pragma warning(disable: 4267)

#include <vector>
#include <algorithm>
#include <cstdint>
#include <mutex>

class AudioBuffer {
public:
    explicit AudioBuffer(size_t capacity = 65536);

    void Clear();
    size_t getReadAvailable() const noexcept;
    size_t Available() const noexcept;
    size_t Read(float* destination, size_t count);
    size_t Write(const float* source, size_t count);
    size_t Capacity() const noexcept;

private:
    std::vector<float> mBuffer;
    size_t mCapacity;
    size_t mReadPosition;
    size_t mWritePosition;
    size_t mSize;
    mutable std::mutex mMutex;
};

#endif // AUDIOBUFFER_H