// Author: Ayvar Aleksiev 2024-2025

#pragma once


#include <array>
#include <bit>
#include <cstdint>
#include <string_view>
#include <iterator>
#include <cstddef>

#include "WeatherStation.h"
#include "Utils.h"

// Hash bucket size. Should be a power of 2, so we can levarage bitwise AND operator instead of modulo.
inline constexpr uint32_t sBucketSize = 2048;

inline constexpr uint32_t sHashMask = sBucketSize - 1;

// Number of hash bits used to select the bucket (log2 of sBucketSize).
inline constexpr int sBucketBits = std::countr_zero(sBucketSize);

/**
 * Hashes a station name from its first 16 bytes.
 *
 * w0 holds bytes 0..7 and w1 holds bytes 8..15 of the name (little-endian), with all bytes
 * at or after the ';' separator zeroed. Names longer than 16 bytes that share the same
 * first 16 bytes collide, which is resolved by linear probing.
 *
 * The multiplication spreads the bits, and the bucket is taken from the top bits of the result.
 */
ALWAYS_INLINE uint64_t HashNameWords(uint64_t w0, uint64_t w1)
{
   return (w0 ^ std::rotl(w1, 17)) * 0x9E3779B97F4A7C15ull;
}

/*
 * A single hash map slot. Aligned to a cache line, so a lookup touches exactly one line.
 * w0/w1 cache the first 16 bytes of the name, so that most key comparisons are two
 * integer compares instead of a memcmp call.
 */
struct alignas(64) WeatherHashMapEntry {
   uint64_t w0{ 0 };
   uint64_t w1{ 0 };
   std::string_view name{};
   WeatherStation::TempStats stats{};
};

/*
 * Iterator Class
 */
class WeatherHashMapIterator {
public:
   using iterator_category = std::forward_iterator_tag;
   using difference_type = std::ptrdiff_t;
   using value_type = WeatherHashMapEntry;
   using pointer = value_type*;
   using reference = value_type&;

   WeatherHashMapIterator(pointer ptr, pointer end) : _ptr(ptr), _end(end) {
      advance_to_valid();
   }

   // Non-const dereference operator
   reference operator*() const { return *_ptr; }
   // Non-const pointer operator
   pointer operator->() { return _ptr; }

   // Prefix increment
   WeatherHashMapIterator& operator++() {
      ++_ptr;
      advance_to_valid();
      return *this;
   }

   // Postfix increment
   WeatherHashMapIterator operator++(int) {
      WeatherHashMapIterator tmp = *this;
      ++(*this);
      return tmp;
   }

   friend bool operator==(const WeatherHashMapIterator& a, const WeatherHashMapIterator& b) {
      return a._ptr == b._ptr;
   }
   friend bool operator!=(const WeatherHashMapIterator& a, const WeatherHashMapIterator& b) {
      return a._ptr != b._ptr;
   }

private:
   inline void advance_to_valid() {
      // Skip empty slots
      while (_ptr != _end && _ptr->name.empty()) {
         ++_ptr;
      }
   }
private:
   pointer _ptr;
   pointer _end;
};


/*
 * The WeatherHashMap class is a specialized, flat hash map designed for high-performance scenarios
 * where the container is expected to hold a relatively small number of elements.
 * This class is particularly optimized for cases where the number of elements is fixed or known in advance,
 * making it an ideal choice when dealing with a predefined set of keys,
 * such as a fixed list of weather stations or cities.
 *
 * Note that, this hash map is not a generic solution.
 * It's purpose is to fit the 413 keys that are expected to be read.
 */
class WeatherHashMap {
public:
   /**
    * Adds a temperature to the station identified by `key`.
    *
    * @param hash  HashNameWords(w0, w1).
    * @param w0    Bytes 0..7 of the name, bytes past the name zeroed.
    * @param w1    Bytes 8..15 of the name, bytes past the name zeroed.
    * @param key   The full name (points into the mapped file).
    * @param value Temperature in tenths of a degree.
    */
   ALWAYS_INLINE void insert_or_assign(
      uint64_t hash, uint64_t w0, uint64_t w1, std::string_view key, int32_t value)
   {
      uint32_t idx = static_cast<uint32_t>(hash >> (64 - sBucketBits));

      // `probe` is only read by the assert, so it is unused in release builds.
      for ([[maybe_unused]] uint32_t probe = 0;; ++probe) {
         WeatherHashMapEntry& entry = _entries[idx];

         // Hot path: the first 16 bytes match. Names up to 16 bytes are fully covered by w0/w1
         // (the size check distinguishes a 16-byte name from a longer one with the same prefix);
         // longer names need a full compare.
         if (entry.w0 == w0 && entry.w1 == w1 &&
             (key.size() <= 16 ? entry.name.size() == key.size() : entry.name == key)) {
            entry.stats.addTemp(value);
            return;
         }
         // Names are never empty, so an empty name marks a free slot.
         if (entry.name.empty()) {
            entry.w0 = w0;
            entry.w1 = w1;
            entry.name = key;
            entry.stats.addTemp(value);
            return;
         }
         // Can only loop forever if the table is full (more keys than sBucketSize).
         // Not expected for 1BRC (413 keys).
         assert(probe < sBucketSize && "WeatherHashMap is full");
         // Linear probing for collision resolution.
         idx = (idx + 1) & sHashMask;
      }
   }

   size_t size() const {
      return std::count_if(
         _entries.begin(), _entries.end(), [](const auto& entry) { return !entry.name.empty(); });
   }

   WeatherHashMapIterator begin() {
      return WeatherHashMapIterator(_entries.data(), _entries.data() + sBucketSize);
   }

   WeatherHashMapIterator end() {
      return WeatherHashMapIterator(_entries.data() + sBucketSize, _entries.data() + sBucketSize);
   }

private:
   std::array<WeatherHashMapEntry, sBucketSize> _entries;
};
