// Author: Ayvar Aleksiev 2024-2025

#include <execution>
#include <iostream>

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/exception/diagnostic_information.hpp> 
#include<boost/container/flat_map.hpp>

#include "CsvReader.h"
#include "WeatherStation.h"
#include "WeatherHashMap.h"

namespace {
   // CSV delimeter
   constexpr char CSV_SEPARATOR = ';';
   constexpr char DECIMAL_SIGN = '.';

   constexpr uint64_t MIN_FILE_SIZE_FOR_ASYNC = 4096;
};

struct MemoryChunk {
   std::string_view _chunkView{};
   uint64_t _idx{};
};

bool
CsvReader::parse(std::string const& file, WeatherStation& result)
{
   try {
      _csvFileRead->open(file);
      if (!_csvFileRead->is_open()) {
         return false;
      }
   }
   catch (boost::exception& ex) {
      std::cout << "Exception caught: " << boost::diagnostic_information(ex) << '\n';
      return false;
   }
   catch (...) {
      return false;
   }
 
   std::string_view view(_csvFileRead->data(), _csvFileRead->size());
   
   if (_csvFileRead->size() >= MIN_FILE_SIZE_FOR_ASYNC) {
      _threadsCount = std::thread::hardware_concurrency();
   }

   uint64_t const chunkSizePerThread = _csvFileRead->size() / _threadsCount;

   processMemoryChunks_(
      splitFileToMemoryChunks_(view, chunkSizePerThread), result);
   return true;
}

std::vector<MemoryChunk>
CsvReader::splitFileToMemoryChunks_(std::string_view& view, uint64_t chunkSize)
{
   std::vector<MemoryChunk> chunksResult;
   chunksResult.reserve(_threadsCount);

   size_t start = 0;
   for (uint64_t i = 0; i < _threadsCount && start < view.size(); ++i) {
      size_t end = std::min(start + chunkSize, view.size());
      // Find the next newline after the proposed end
      while (end < view.size() && view[end] != '\n') ++end;
      if (end < view.size()) ++end; // Include the newline

      chunksResult.emplace_back(std::string_view{ &view[start], end - start }, i);
      start = end;
   }

   return chunksResult;
}

void
CsvReader::processMemoryChunks_(
   std::vector<MemoryChunk>&& fileChunks, WeatherStation& result)
{
   std::vector<WeatherHashMap> intermediateResults(_threadsCount);

   std::for_each(std::execution::par_unseq, fileChunks.begin(), fileChunks.end(), [&intermediateResults, this](struct MemoryChunk const& chunk) {

      const char* cur = chunk._chunkView.data();
      const char* end = cur + chunk._chunkView.size();

      while (cur < end) {
         const char* cityNamePtr = cur; // marks start of city name
         hash_t h = 0;
         // Reads the city name character-by-character and incrementally hashes its content.
         while (*cur != CSV_SEPARATOR) {
            h = FastCharacterHash16Func(*cur, h);
            ++cur;
         }
         std::string_view cityView(cityNamePtr, cur - cityNamePtr);

         // skip csv separator ';' and move to floating point number starting position.
         ++cur;
         intermediateResults[chunk._idx].insert_or_assign(h, cityView, parseDecimalNumber_(cur));
         // Advance the pointer to the character after '\n' or at `end` EOF.
         ++cur;
      }
   });

   for (auto& umap : intermediateResults) {
      for (auto const& [cityView, temps] : umap) {
         result.addTemperature(cityView, temps._min, temps._max, temps._sum, temps._count);
      }
   }
}


