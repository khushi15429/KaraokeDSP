#include "AudioBuffer.h"

AudioBuffer::AudioBuffer(size_t capacity)
    : mBuffer(capacity),
    mCapacity(capacity),
    mReadPosition(0),
    mWritePosition(0),
    mSize(0)
{
}

void AudioBuffer::Clear()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mReadPosition = 0;
    mWritePosition = 0;
    mSize = 0;
    std::fill(mBuffer.begin(), mBuffer.end(), 0.0f);
}

size_t AudioBuffer::getReadAvailable() const noexcept
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mSize;
}

size_t AudioBuffer::Available() const noexcept
{
    return getReadAvailable();
}

size_t AudioBuffer::Read(float* destination, size_t count)
{
    if (!destination || count == 0) return 0;

    std::lock_guard<std::mutex> lock(mMutex);
    size_t toRead = std::min(count, mSize);

    for (size_t i = 0; i < toRead; ++i) {
        destination[i] = mBuffer[mReadPosition];
        mReadPosition = (mReadPosition + 1) % mCapacity;
    }

    mSize -= toRead;
    return toRead;
}

size_t AudioBuffer::Write(const float* source, size_t count)
{
    if (!source || count == 0) return 0;

    std::lock_guard<std::mutex> lock(mMutex);
    size_t availableSpace = mCapacity - mSize;
    size_t toWrite = std::min(count, availableSpace);

    for (size_t i = 0; i < toWrite; ++i) {
        mBuffer[mWritePosition] = source[i];
        mWritePosition = (mWritePosition + 1) % mCapacity;
    }

    mSize += toWrite;
    return toWrite;
}

size_t AudioBuffer::Capacity() const noexcept
{
    return mCapacity;
}