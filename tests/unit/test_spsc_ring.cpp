#include <catch2/catch_test_macros.hpp>
#include <chorus/playback/spsc_ring.hpp>

#include <atomic>
#include <thread>
#include <vector>

namespace {
constexpr size_t kSmallCapacity = 16;
constexpr size_t kWraparoundCapacity = 8;
constexpr size_t kStressCapacity = 1024;
constexpr size_t kStressTotalItems = 100000;
}  // namespace

TEST_CASE("SpscRing basic read and write", "[playback][spsc]") {
    chorus::SpscRing<int> ring(kSmallCapacity);
    REQUIRE(ring.capacity() == kSmallCapacity);
    REQUIRE(ring.size() == 0);
    REQUIRE(ring.available_write() == kSmallCapacity);

    const std::vector<int> input = {1, 2, 3, 4, 5};
    const size_t written = ring.write(input);
    REQUIRE(written == input.size());
    REQUIRE(ring.size() == input.size());
    REQUIRE(ring.available_write() == (kSmallCapacity - input.size()));

    std::vector<int> output(3, 0);
    size_t read_count = ring.read(output);
    REQUIRE(read_count == 3);
    REQUIRE(output == std::vector<int>{1, 2, 3});
    REQUIRE(ring.size() == 2);

    std::vector<int> output2(5, 0);
    read_count = ring.read(output2);
    REQUIRE(read_count == 2);
    REQUIRE(output2[0] == 4);
    REQUIRE(output2[1] == 5);
    REQUIRE(ring.size() == 0);
}

TEST_CASE("SpscRing wraparound behavior", "[playback][spsc]") {
    chorus::SpscRing<int> ring(kWraparoundCapacity);

    // Fill 6 elements
    const std::vector<int> first = {10, 20, 30, 40, 50, 60};
    REQUIRE(ring.write(first) == first.size());

    // Read 4 elements
    std::vector<int> out1(4);
    REQUIRE(ring.read(out1) == out1.size());
    REQUIRE(out1 == std::vector<int>{10, 20, 30, 40});

    // Write 5 elements (causing wraparound)
    const std::vector<int> second = {70, 80, 90, 100, 110};
    REQUIRE(ring.write(second) == second.size());

    // Read all remaining elements
    std::vector<int> out2(7);
    REQUIRE(ring.read(out2) == out2.size());
    REQUIRE(out2 == std::vector<int>{50, 60, 70, 80, 90, 100, 110});
    REQUIRE(ring.size() == 0);
}

TEST_CASE("SpscRing multi-threaded producer-consumer stress test", "[playback][spsc]") {
    chorus::SpscRing<uint64_t> ring(kStressCapacity);
    std::atomic<bool> start_flag{false};

    std::jthread producer([&]() {
        while (!start_flag.load(std::memory_order_relaxed)) {
            std::this_thread::yield();
        }

        size_t current = 0;
        std::vector<uint64_t> chunk(64);
        while (current < kStressTotalItems) {
            const size_t count = std::min<size_t>(chunk.size(), kStressTotalItems - current);
            for (size_t i = 0; i < count; ++i) {
                chunk[i] = current + i;
            }

            size_t written = 0;
            while (written < count) {
                written += ring.write(std::span<const uint64_t>(chunk.data() + written, count - written));
                if (written < count) {
                    std::this_thread::yield();
                }
            }
            current += count;
        }
    });

    std::vector<uint64_t> received;
    received.reserve(kStressTotalItems);

    start_flag.store(true, std::memory_order_release);

    std::vector<uint64_t> read_chunk(128);
    while (received.size() < kStressTotalItems) {
        const size_t read_count = ring.read(read_chunk);
        if (read_count > 0) {
            received.insert(received.end(), read_chunk.begin(), read_chunk.begin() + static_cast<ptrdiff_t>(read_count));
        } else {
            std::this_thread::yield();
        }
    }

    REQUIRE(received.size() == kStressTotalItems);
    for (size_t i = 0; i < kStressTotalItems; ++i) {
        if (received[i] != i) {
            FAIL("Mismatch at index " << i << ": expected " << i << " got " << received[i]);
        }
    }
}
