// Author: Ayvar Aleksiev 2024

#pragma once

#include <string_view>
#include <array>

#include <iterator>
#include <cstddef>

#include "WeatherStation.h"

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

/*struct FastHash16Func {
   hash_t operator()(std::string_view sv) const {
      // Compute rotate-and-ADD/XOR hash
      hash_t h = 0;
      for (uint8_t ch : sv) {
         // Perform a bitwise rotation (also known as a "circular shift") on the variable.
         h = (h << rotateAmount) | (h >> (sizeof(h) * 8 - rotateAmount));
         h = h HASH_OP hash_t(ch);
      }
      return h;
   }
};*/

struct FastCharacterHash16Func {
   void operator()(uint8_t ch, hash_t& h) {
      h = (h << rotateAmount) | (h >> (sizeof(h) * 8 - rotateAmount));
      h = h HASH_OP hash_t(ch);
   }
};

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

   void insert_or_assign(hash_t h, std::string_view key, int32_t value) {

      KeyValuePair* slot = &_flatMap[h & hashMask];

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
            slot = &_flatMap[h & hashMask];
         }
      }
   }

   /*
   WeatherStation::TempStats& operator[](std::string_view key) {
   
      hash_t h = FastHash16Func()(key);
   
      KeyValuePair* slot = &_flatMap[h & hashMask];
   
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
            slot = &_flatMap[h & hashMask];
         }
      }
   }
   */

   size_t size() const {
      size_t result = 0;
      for (auto const& pair : _flatMap) {
         if (!pair.first.empty()) {
            ++result;
         }
      }
      return result;
   }

   WeatherHashMapIterator begin() {
      return WeatherHashMapIterator(_flatMap.data(), _flatMap.data() + BucketSize);
   }

   WeatherHashMapIterator end() {
      return WeatherHashMapIterator(_flatMap.data() + BucketSize, _flatMap.data() + BucketSize);
   }

private:
   std::array <KeyValuePair, BucketSize> _flatMap;
};
