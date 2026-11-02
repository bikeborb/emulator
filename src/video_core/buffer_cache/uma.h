// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <span>
#include <vector>

#include "common/common_types.h"

namespace VideoCommon::Uma {

/// Unified memory upload rule for the buffer cache.
///
/// On an integrated GPU whose buffers can live in memory that is device local, host visible
/// and host coherent (MemoryAllocator::CreateUnifiedBuffer), the CPU can write guest data
/// straight into a cached buffer instead of filling a staging buffer and recording a GPU copy.
/// That is only correct when no GPU work that was handed those bytes can still read or write
/// them.
///
/// Each buffer keeps one stamp per 4 KiB page: the scheduler tick that was current when GPU
/// work was last handed that page (bound, copied, cleared, obtained). The command that uses it
/// may be recorded into the following tick if the scheduler flushes in between, so the rule
/// waits for one more tick than the stamp: the GPU must have finished tick `stamp + 1`.
/// Pages, not whole buffers, because the guest keeps writing small ranges of large buffers
/// whose other ranges are in use (measured on the Thor: about two thirds of the uploads a
/// whole-buffer stamp sent to staging touch only idle pages).
constexpr u64 PAGE_BITS = 12;

[[nodiscard]] constexpr u64 PageCount(u64 size_bytes) noexcept {
    return (size_bytes + (u64{1} << PAGE_BITS) - 1) >> PAGE_BITS;
}

/// Records that GPU work recorded at `tick` uses [offset, offset + size).
constexpr void StampPages(std::span<u64> page_ticks, u64 offset, u64 size, u64 tick) noexcept {
    if (size == 0) {
        return;
    }
    const u64 first = offset >> PAGE_BITS;
    if (first >= page_ticks.size()) {
        return;
    }
    const u64 last = std::min<u64>(page_ticks.size() - 1, (offset + size - 1) >> PAGE_BITS);
    std::fill(page_ticks.begin() + first, page_ticks.begin() + last + 1, tick);
}

/// True when the CPU may write [offset, offset + size) of a mapped buffer directly: every page
/// it touches was last handed to GPU work in a tick the GPU has finished, plus one.
[[nodiscard]] constexpr bool CanWriteDirectly(bool is_mapped, std::span<const u64> page_ticks,
                                              u64 offset, u64 size, u64 known_gpu_tick) noexcept {
    if (!is_mapped) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    const u64 first = offset >> PAGE_BITS;
    const u64 last = (offset + size - 1) >> PAGE_BITS;
    if (last >= page_ticks.size()) {
        return false;
    }
    for (u64 page = first; page <= last; ++page) {
        if (known_gpu_tick <= page_ticks[page]) {
            return false;
        }
    }
    return true;
}

/// The page stamps of one buffer, with the stamps of the current tick merged before they are
/// written. A vertex stream is handed to GPU work with its whole range, often megabytes, on
/// every draw that rebinds it; stamping that range each time was about 80 million page stamps a
/// second in the opening of Red Dead Redemption (desktop build), nearly all of them the same
/// pages with the same tick. Stamps of one tick that overlap or touch are kept as one pending
/// range, written once when a stamp of another tick or a range apart from it comes in.
/// CanWriteDirectly reads the pending range as well, so nothing is lost by waiting.
class PageTicks {
public:
    PageTicks() = default;
    explicit PageTicks(u64 size_bytes) : ticks(PageCount(size_bytes), 0) {}

    /// StampPages for this buffer.
    void Stamp(u64 offset, u64 size, u64 tick) noexcept {
        if (size == 0) {
            return;
        }
        const u64 first = offset >> PAGE_BITS;
        if (first >= ticks.size()) {
            return;
        }
        const u64 end = std::min<u64>(ticks.size(), ((offset + size - 1) >> PAGE_BITS) + 1);
        if (tick == pending_tick && pending_first != pending_end && first <= pending_end &&
            end >= pending_first) {
            pending_first = std::min(pending_first, first);
            pending_end = std::max(pending_end, end);
            return;
        }
        Flush();
        pending_tick = tick;
        pending_first = first;
        pending_end = end;
    }

    /// Uma::CanWriteDirectly for this buffer.
    [[nodiscard]] bool CanWriteDirectly(bool is_mapped, u64 offset, u64 size,
                                        u64 known_gpu_tick) const noexcept {
        if (is_mapped && size != 0 && pending_first != pending_end &&
            known_gpu_tick <= pending_tick) {
            const u64 first = offset >> PAGE_BITS;
            const u64 last = (offset + size - 1) >> PAGE_BITS;
            if (first < pending_end && last >= pending_first) {
                return false;
            }
        }
        return Uma::CanWriteDirectly(is_mapped, ticks, offset, size, known_gpu_tick);
    }

private:
    void Flush() noexcept {
        if (pending_first != pending_end) {
            std::fill(ticks.begin() + pending_first, ticks.begin() + pending_end, pending_tick);
        }
        pending_first = 0;
        pending_end = 0;
    }

    std::vector<u64> ticks;
    u64 pending_tick = 0;
    u64 pending_first = 0; ///< First page of the pending range
    u64 pending_end = 0;   ///< One past its last page; equal to pending_first when empty
};

} // namespace VideoCommon::Uma
