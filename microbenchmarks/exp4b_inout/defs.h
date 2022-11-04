#pragma once
#include <cstdint>

constexpr uint64_t NUM_LATENCIES = 1e6;
constexpr uint64_t OFFSET = 100;
constexpr uint64_t PAGE_SIZE = 4096;

/**
 * @brief Structure that represents a 4KB page
 *
 */
struct Node {
   Node* next{nullptr};    // 8 Byte pointer
   uint64_t data{0};       // 8 Byte data
   uint64_t padding[510];  // (4096-8-8)/8=510 to create a 4KB page
};