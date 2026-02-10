#pragma once
#include <atomic>
#include <concepts>
#include <utility>

template <typename T>
class DoubleBuffer {
public:
    DoubleBuffer() : read_idx(0), dirty(false) {}

    explicit DoubleBuffer(const T& initialValue) : read_idx(0), dirty(false) {
        buffers[0] = initialValue;
        buffers[1] = initialValue;
    }

    // safe on ui thread
    T& write() {
        return buffers[1 - read_idx.load(std::memory_order_acquire)];
    }

    // safe on ui thread
    void mark_dirty() {
        dirty.store(true, std::memory_order_release);
    }

    // safe on audio thread
    const T& read() {
        if (dirty.load(std::memory_order_acquire)) {
            int write_idx = 1 - read_idx.load(std::memory_order_relaxed);
            buffers[read_idx.load(std::memory_order_relaxed)] = buffers[write_idx];
            dirty.store(false, std::memory_order_relaxed);
        }
        return buffers[read_idx.load(std::memory_order_relaxed)];
    }

    // read without flipping if dirty - safe on either thread
    const T& peek() const {
        return buffers[read_idx.load(std::memory_order_relaxed)];
    }

    // set both buffers to the same value - safe on ui thread
    void setAll(const T& value) {
        buffers[0] = value;
        buffers[1] = value;
        dirty.store(false, std::memory_order_release);
    }

private:
    T buffers[2];
    std::atomic<int> read_idx;
    std::atomic<bool> dirty;
};
