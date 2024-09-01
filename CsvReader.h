// Author: Ayvar Aleksiev 2024

#pragma once

#include <memory>
#include <string_view>
#include <boost/iostreams/device/mapped_file.hpp>


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
      splitFileToMemoryChunks_(std::string_view& view, uint64_t chunkSizePerThread);

   // Process the memory chunks and creates the final version of WeatherStation.
   void processMemoryChunks_(
      std::vector<MemoryChunk>&& fileChunks, WeatherStation& result);

   // Parses only one decimal numbers of type "x.y", "xx.y", "-x.y", "-xx.y".
   inline int parseDecimalNumber(const char* s)
   {
      // parse sign
      int sign = 1;
      if (*s == '-') {
         sign = -1;
         s++;
      }
      // Case 1: "3.4"
      if (s[1] == '.') {
         return ((s[0] - '0') * 10 + (s[2] - '0')) * sign;
      }
      // Case 2: "37.4"
      return ((s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[3] - '0')) * sign;
   }

private:
   using fileColse = decltype(
      [](mapped_file_source* file) { 
         file->close();
      });
   std::unique_ptr<mapped_file_source, fileColse> _csvFileRead{ new mapped_file_source };

   uint64_t _threadsCount{1};
};