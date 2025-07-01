// Author: Ayvar Aleksiev 2024-2025

#pragma once

#include <string_view>
#include <array>

#include <iterator>
#include <cstddef>

#include "WeatherStation.h"
#include "Utils.h"

#define HASH_OP ^

// Data type to use for the hash
using hash_t = uint16_t;

// Hash bucket size. Should be a power of 2, so we can levarage bitwise AND operator instead of modulo.
inline constexpr int32_t BucketSize = 2048;

// Number of steps to perform for linear probing.
inline constexpr uint16_t probeLimit = 3;

// Bits to rotate left after each step
inline constexpr int32_t rotateAmount = 13;

inline constexpr hash_t hashMask = hash_t(BucketSize - 1);

/**
 * Performs a single step fast 16-bit character hash.
 *
 * This function is intended for use in incremental hashing, where the input
 * is read character-by-character (e.g., from a file or stream). It takes a single
 * character and the current hash value, then applies a bitwise rotate-left followed by
 * a mixing operation (e.g., XOR), returning the updated hash.
 *
 * The hashing approach is fast and simple, making it suitable for small keys
 * or lightweight parsing scenarios such as tokenizing CSV fields.
 * 
 * Typical usage:
 * @code
 * hash_t h = 0;
 * while (*cur != ';') {
 *     h = FastCharacterHash16Func(*cur, h);
 *     ++cur;
 * }
 * @endcode
 *
 * @param ch The input character to mix into the hash.
 * @param h  The current hash value. Typically initialized to zero at the start of hashing.
 * @return   The updated hash value after incorporating the character.
 */
BEGIN_HOT_SEGMENT
ALWAYS_INLINE hash_t FastCharacterHash16Func(uint8_t ch, hash_t h)
{
   h = (h << rotateAmount) | (h >> (sizeof(h) * 8 - rotateAmount));
   return h HASH_OP hash_t(ch);
}
END_HOT_SEGMENT

// Size is 32 bytes.
using KeyValuePair = std::pair<std::string_view, WeatherStation::TempStats>;

/* 
 * Iterator Class
 */
class WeatherHashMapIterator {
public:
   using iterator_category = std::forward_iterator_tag;
   using difference_type = std::ptrdiff_t;
   using value_type = KeyValuePair;
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
      while (_ptr != _end && _ptr->first.empty()) {
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
   BEGIN_HOT_SEGMENT
   ALWAYS_INLINE void insert_or_assign(hash_t h, std::string_view key, int32_t value) {

      KeyValuePair* slot = &_kvEntries[h & hashMask];

      // Handling custom scenario with expected keys.
      // If the keys are different this may end as an infinite loop.
      for (uint16_t probe = 0;;) {
         if (slot->first.empty()) {
            // empty slot -> claim it
            slot->first = key;
            slot->second.addTemp(value);
            return;
         }
         else if (slot->first == key) {
            // correct slot found
            slot->second.addTemp(value);
            return;
         }
         else if (probe < probeLimit) {
            // Try linear probing for collision resolution
            h += (++probe);
            slot = &_kvEntries[h & hashMask];
         }
      }
   }
   END_HOT_SEGMENT

   /*
   WeatherStation::TempStats& operator[](std::string_view key) {
   
      hash_t h = FastHash16Func()(key);
   
      KeyValuePair* slot = &_kvEntries[h & hashMask];
   
      // Handling custom scenario with expected keys.
      // If the keys are different this may end as an infinite loop.
      for (uint16_t probe = 0;;) {
         if (slot->first.empty()) {
            // empty slot -> claim it
            slot->first = key;
            return slot->second;
         }
         else if (slot->first == key) {
            // correct slot found
            return slot->second;
         }
         else if (probe < probeLimit) {
            // Try linear probing for collision resolution
            h += (++probe);
            slot = &_kvEntries[h & hashMask];
         }
      }
   }
   */

   size_t size() const {
      return std::count_if(
         _kvEntries.begin(), _kvEntries.end(), [](const auto& pair) { return !pair.first.empty(); });
   }

   WeatherHashMapIterator begin() {
      return WeatherHashMapIterator(_kvEntries.data(), _kvEntries.data() + BucketSize);
   }

   WeatherHashMapIterator end() {
      return WeatherHashMapIterator(_kvEntries.data() + BucketSize, _kvEntries.data() + BucketSize);
   }

private:
   std::array <KeyValuePair, BucketSize> _kvEntries;
};
