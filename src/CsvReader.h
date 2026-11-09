// Author: Ayvar Aleksiev 2024

#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <boost/iostreams/device/mapped_file.hpp>

#include "Utils.h"

class WeatherStation;
struct MemoryChunk;

using boost::iostreams::mapped_file_source;

class CsvReader
{
public:
   bool parse(std::string const& file, WeatherStation& result);

private:

   // Splitting file to memory chunks.
   std::vector<MemoryChunk>
      splitFileToMemoryChunks_(std::string_view& view, uint64_t chunkSize);

   // Process the memory chunks and creates the final version of WeatherStation.
   void processMemoryChunks_(
      std::vector<MemoryChunk>&& fileChunks, WeatherStation& result);

   // Parses a single decimal number of the form "x.y", "xx.y", "-x.y", or "-xx.y",
   // and advances the pointer to the character immediately after the parsed number (ideally '\n').
   ALWAYS_INLINE int parseDecimalNumber_(const char*& curPtr)
   {
      // parse sign
      int sign = 1;
      if (*curPtr == '-') {
         sign = -1;
         ++curPtr;
      }
      static constexpr char DECIMAL_SIGN = '.';
      // Case 1: "3.4"
      if (curPtr[1] == DECIMAL_SIGN) {
         int value = sign * ((curPtr[0] - '0') * 10 + (curPtr[2] - '0'));
         curPtr += 3; // Move past "3.4"
         return value;
      }
      // Case 2: "37.4"
      int value = ((curPtr[0] - '0') * 100 + (curPtr[1] - '0') * 10 + (curPtr[3] - '0')) * sign;
      curPtr += 4; // Move past "37.4"
      return value;
   }

   // Branchless version of parseDecimalNumber_ (technique by Quan Anh Mai, 1BRC).
   // Parses "x.y", "xx.y", "-x.y" or "-xx.y" from a single 8-byte load, with no branches on
   // the sign or the number of digits, and advances the pointer to the START OF THE NEXT LINE
   // (past the '\n'), unlike parseDecimalNumber_ which stops at the '\n'.
   // Reads 8 bytes from curPtr, so at least 8 bytes must be readable. Requires little-endian.
   ALWAYS_INLINE int parseDecimalNumberBranchless_(const char*& curPtr)
   {
      uint64_t word;
      std::memcpy(&word, curPtr, sizeof(word));

      // Digits (0x30-0x39) have bit 4 set, '.' (0x2E) does not. Check bytes 1, 2, 3 for the '.'.
      // Result: 12 if '.' is byte 1, 20 if byte 2, 28 if byte 3.
      const int decimalSepPos = std::countr_zero(~word & 0x10101000ull);

      // '-' (0x2D) also has bit 4 clear: all ones if negative, else zero.
      const int64_t negMask = static_cast<int64_t>(~word << 59) >> 63;

      // Clear the '-' byte, then shift so the digits always land in the same positions:
      // tens in byte 1 (or 0 if absent), units in byte 2, tenths in byte 4.
      const uint64_t designMask = ~(static_cast<uint64_t>(negMask) & 0xFF);
      const uint64_t digits = ((word & designMask) << (28 - decimalSepPos)) & 0x0F000F0F00ull;

      // One multiplication computes tens*100 + units*10 + tenths into bits 32..41.
      const int64_t absValue = static_cast<int64_t>(((digits * 0x640a0001ull) >> 32) & 0x3FF);

      // Apply the sign: (x ^ -1) - (-1) == -x, (x ^ 0) - 0 == x.
      const int value = static_cast<int>((absValue ^ negMask) - negMask);

      // Index of the '.' byte + 1 (tenths digit) + 1 ('\n') + 1 (next line).
      curPtr += (decimalSepPos >> 3) + 3;
      return value;
   }

private:
   mapped_file_source _csvFileRead;

   uint64_t _threadsCount{1};
};
