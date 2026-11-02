// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstring>
#include <vector>

#include "common/alignment.h"
#include "common/common_types.h"

namespace VideoCommon {

class UsageTracker {
    static constexpr size_t BYTES_PER_BIT_SHIFT = 6;
    static constexpr size_t PAGE_SHIFT = 6 + BYTES_PER_BIT_SHIFT;
    static constexpr size_t PAGE_BYTES = 1 << PAGE_SHIFT;

public:
    explicit UsageTracker(size_t size) {
        const size_t num_pages = (size >> PAGE_SHIFT) + 1;
        pages.resize(num_pages, 0ULL);
        dirty_first = num_pages;
    }

    /// Forgets all usage. Clears only the pages written since the last reset: the buffer is
    /// reset at every new scheduler tick it is used in, and most ticks touch a small part of it.
    void Reset() noexcept {
        if (dirty_first < dirty_end) {
            std::fill(pages.begin() + dirty_first, pages.begin() + dirty_end, 0ULL);
        }
        dirty_first = pages.size();
        dirty_end = 0;
        pending_offset = 0;
        pending_end = 0;
    }

    /// Records that [offset, offset + size) is used. Usage only grows until the next reset, so
    /// a range that overlaps or touches the pending one is merged into it instead of written:
    /// a vertex stream is tracked with its whole range, often megabytes, on every draw that
    /// rebinds it. The pending range is written when a range apart from it comes in.
    void Track(u64 offset, u64 size) noexcept {
        const size_t page = offset >> PAGE_SHIFT;
        const size_t page_end = (offset + size) >> PAGE_SHIFT;
        if (size == 0 || page_end < page || page_end >= pages.size()) {
            return;
        }
        const u64 end = offset + size;
        if (pending_offset != pending_end && offset <= pending_end && end >= pending_offset) {
            pending_offset = std::min(pending_offset, offset);
            pending_end = std::max(pending_end, end);
            return;
        }
        Flush();
        pending_offset = offset;
        pending_end = end;
    }

    [[nodiscard]] bool IsUsed(u64 offset, u64 size) const noexcept {
        const size_t page = offset >> PAGE_SHIFT;
        const size_t page_end = (offset + size) >> PAGE_SHIFT;
        if (page_end < page || page_end >= pages.size()) {
            return false;
        }
        // The pending range at the granularity of the pages: one bit per 64 bytes.
        if (size != 0 && pending_offset != pending_end) {
            constexpr u64 granule = u64{1} << BYTES_PER_BIT_SHIFT;
            if (Common::AlignDown(offset, granule) < Common::AlignUp(pending_end, granule) &&
                Common::AlignUp(offset + size, granule) >
                    Common::AlignDown(pending_offset, granule)) {
                return true;
            }
        }
        return IsUsedInPages(offset, size);
    }

private:
    void Flush() noexcept {
        if (pending_offset == pending_end) {
            return;
        }
        TrackRange(pending_offset, pending_end - pending_offset);
        pending_offset = 0;
        pending_end = 0;
    }

    void TrackRange(u64 offset, u64 size) noexcept {
        const size_t page = offset >> PAGE_SHIFT;
        const size_t page_end = (offset + size) >> PAGE_SHIFT;
        dirty_first = std::min(dirty_first, page);
        dirty_end = std::max(dirty_end, page_end + 1);
        TrackPage(page, offset, size);
        if (page == page_end) {
            return;
        }
        // A vertex stream is bound with its whole range on every draw that changes it, which makes
        // this megabytes of buffer per call. Fill the middle in one go.
        std::memset(pages.data() + page + 1, 0xFF, (page_end - page - 1) * sizeof(u64));
        const size_t offset_end = offset + size;
        const size_t offset_end_page_aligned = Common::AlignDown(offset_end, PAGE_BYTES);
        TrackPage(page_end, offset_end_page_aligned, offset_end - offset_end_page_aligned);
    }

    [[nodiscard]] bool IsUsedInPages(u64 offset, u64 size) const noexcept {
        const size_t page = offset >> PAGE_SHIFT;
        const size_t page_end = (offset + size) >> PAGE_SHIFT;
        if (page_end < page || page_end >= pages.size()) {
            return false;
        }
        if (IsPageUsed(page, offset, size)) {
            return true;
        }
        for (size_t i = page + 1; i < page_end; i++) {
            if (pages[i] != 0) {
                return true;
            }
        }
        const size_t offset_end = offset + size;
        const size_t offset_end_page_aligned = Common::AlignDown(offset_end, PAGE_BYTES);
        return IsPageUsed(page_end, offset_end_page_aligned, offset_end - offset_end_page_aligned);
    }

    /// The bits of one page for the part of [offset, offset + size) inside that page, rounded
    /// out to whole 64-byte units. (This used to count size >> 6 bits from the first one, which
    /// left out a partial last unit and, under 64 bytes, shifted a u64 by 64.)
    [[nodiscard]] static u64 PageMask(u64 offset, u64 size) noexcept {
        const u64 offset_in_page = offset % PAGE_BYTES;
        const u64 end_in_page = std::min<u64>(offset_in_page + size, PAGE_BYTES);
        if (size == 0 || end_in_page <= offset_in_page) {
            return 0;
        }
        const u64 first_bit = offset_in_page >> BYTES_PER_BIT_SHIFT;
        const u64 last_bit = (end_in_page - 1) >> BYTES_PER_BIT_SHIFT;
        const u64 count = last_bit - first_bit + 1;
        return (count == 64 ? ~u64{0} : (u64{1} << count) - 1) << first_bit;
    }

    void TrackPage(u64 page, u64 offset, u64 size) noexcept {
        pages[page] |= PageMask(offset, size);
    }

    [[nodiscard]] bool IsPageUsed(u64 page, u64 offset, u64 size) const noexcept {
        return (pages[page] & PageMask(offset, size)) != 0;
    }

    std::vector<u64> pages;
    u64 pending_offset = 0; ///< Pending range, not yet written to the pages; empty when equal
    u64 pending_end = 0;
    size_t dirty_first = 0; ///< Pages written since the last reset: [dirty_first, dirty_end)
    size_t dirty_end = 0;
};

} // namespace VideoCommon
