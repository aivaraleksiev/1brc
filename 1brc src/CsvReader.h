// Author: Ayvar Aleksiev 2024

#pragma once

#include <memory>
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
      // Case 1: "3.4"
      if (curPtr[1] == '.') {
         int value = sign * ((curPtr[0] - '0') * 10 + (curPtr[2] - '0'));
         curPtr += 3; // Move past "3.4"
         return value;
      }
      // Case 2: "37.4"
      int value = ((curPtr[0] - '0') * 100 + (curPtr[1] - '0') * 10 + (curPtr[3] - '0')) * sign;
      curPtr += 4; // Move past "37.4"
      return value;
   }

private:
   using fileColse = decltype(
      [](mapped_file_source* file) { 
         file->close();
      });
   std::unique_ptr<mapped_file_source, fileColse> _csvFileRead{ new mapped_file_source };

   uint64_t _threadsCount{1};
};
