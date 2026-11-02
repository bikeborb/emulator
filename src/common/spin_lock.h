// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <mutex>

namespace Common {

/**
 * SpinLock class
 * a lock similar to mutex that forces a thread to spin wait instead calling the
 * supervisor. Should be used on short sequences of code.
 */
class SpinLock {
public:
    SpinLock() = default;

    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    SpinLock(SpinLock&&) = delete;
    SpinLock& operator=(SpinLock&&) = delete;

    void lock();
    void unlock();
    [[nodiscard]] bool try_lock();

private:
    std::atomic_flag lck = ATOMIC_FLAG_INIT;
};

/**
 * A recursive mutex that keeps trying for a short while before it puts the thread to sleep.
 * For locks held for microseconds by threads that collide often, where sleeping and being woken
 * again costs far more than the wait itself. Measured on the AYN Thor (Red Dead Redemption,
 * Armadillo Station): the GPU thread slept about 1,200 times a second on the buffer and texture
 * cache locks, which guest write faults take for a few microseconds, and spent 17% of its time
 * asleep there, about 140 microseconds per wait.
 */
class SpinningRecursiveMutex {
public:
    SpinningRecursiveMutex() = default;

    SpinningRecursiveMutex(const SpinningRecursiveMutex&) = delete;
    SpinningRecursiveMutex& operator=(const SpinningRecursiveMutex&) = delete;

    void lock();
    void unlock() {
        mutex.unlock();
    }
    [[nodiscard]] bool try_lock() {
        return mutex.try_lock();
    }

private:
    std::recursive_mutex mutex;
};

} // namespace Common
