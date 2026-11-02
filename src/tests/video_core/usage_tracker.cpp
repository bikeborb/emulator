// SPDX-FileCopyrightText: Copyright 2026 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "video_core/buffer_cache/usage_tracker.h"

using VideoCommon::UsageTracker;

TEST_CASE("UsageTracker: tracked bytes are used, others are not", "[video_core][usage_tracker]") {
    UsageTracker tracker(1u << 20);
    tracker.Track(10000, 5000);
    REQUIRE(tracker.IsUsed(10000, 1));
    REQUIRE(tracker.IsUsed(14999, 1));
    REQUIRE(tracker.IsUsed(9000, 2000)); // overlaps the start
    REQUIRE_FALSE(tracker.IsUsed(0, 9900));
    REQUIRE_FALSE(tracker.IsUsed(15104, 4096)); // past the end, beyond its 64-byte unit
}

TEST_CASE("UsageTracker: ranges under 64 bytes and partial units are tracked",
          "[video_core][usage_tracker]") {
    // Used to count size >> 6 bits from the first unit: a 40-byte range was a shift by 64 and
    // the partial last unit of a range was left out.
    UsageTracker tracker(1u << 16);
    tracker.Track(128, 40);
    REQUIRE(tracker.IsUsed(160, 4));
    tracker.Track(8192, 100); // apart: the first range is written to the pages
    REQUIRE(tracker.IsUsed(128, 1));
    REQUIRE(tracker.IsUsed(8192 + 96, 4)); // the second 64-byte unit of the range
    tracker.Track(40000, 1);               // apart again: the second range is written as well
    REQUIRE(tracker.IsUsed(8192 + 96, 4));
    REQUIRE_FALSE(tracker.IsUsed(4096, 64));
    REQUIRE_FALSE(tracker.IsUsed(8192 + 4096, 64));
}

TEST_CASE("UsageTracker: merged and written ranges answer the same",
          "[video_core][usage_tracker]") {
    UsageTracker tracker(1u << 20);
    tracker.Track(0, 300000);
    tracker.Track(100000, 400000); // overlaps: merged into one pending range
    tracker.Track(800000, 1000);   // apart: [0, 500000) is written
    for (u64 offset = 0; offset < (1u << 20) - 4096; offset += 4096) {
        const bool expected = offset < 500000 || (offset + 4096 > 800000 && offset < 801000);
        REQUIRE(tracker.IsUsed(offset, 4096) == expected);
    }
}

TEST_CASE("UsageTracker: reset forgets pending and written ranges", "[video_core][usage_tracker]") {
    UsageTracker tracker(1u << 20);
    tracker.Track(4096, 4096);
    tracker.Track(65536, 64); // apart: the first is written
    tracker.Reset();
    REQUIRE_FALSE(tracker.IsUsed(0, 1u << 19));
    tracker.Track(500000, 10);
    REQUIRE(tracker.IsUsed(500000, 1));
    tracker.Reset();
    REQUIRE_FALSE(tracker.IsUsed(499968, 128));
}

TEST_CASE("UsageTracker: ranges outside the buffer are ignored", "[video_core][usage_tracker]") {
    UsageTracker tracker(8192);
    tracker.Track(1u << 20, 64);
    REQUIRE_FALSE(tracker.IsUsed(1u << 20, 64));
    tracker.Track(0, 64);
    REQUIRE_FALSE(tracker.IsUsed(1u << 20, 64)); // a query outside the buffer is never used
    REQUIRE(tracker.IsUsed(0, 64));
}
