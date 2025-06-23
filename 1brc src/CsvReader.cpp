// Author: Ayvar Aleksiev 2024

#include <execution>

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
   for (uint64_t i = 0; i < _threadsCount; ++i) {
      size_t end = std::min(start + chunkSize, view.size());
      // Find the next newline after the proposed end
      while (end < view.size() && view[end] != '\n') ++end;
      if (end < view.size()) ++end; // Include the newline

      chunksResult.emplace_back(std::string_view{ &view[start], end - start }, i);
      start = end;
      if (start >= view.size()) { break; }
   }

   return chunksResult;
}

void
CsvReader::processMemoryChunks_(
   std::vector<MemoryChunk>&& fileChunks, WeatherStation& result)
{
   std::vector<WeatherHashMap> intermediateResults(_threadsCount);

   std::for_each(std::execution::par_unseq, fileChunks.begin(), fileChunks.end(), [&](struct MemoryChunk const& chunk) {

      auto chunkView = chunk._chunkView;
      auto const chunkIdx = chunk._idx;

      while (!chunkView.empty()) {
         const char* chunkPtr = chunkView.data();
         hash_t h = 0;
         size_t csvSeparatorPos = 0;
         // Find CSV separator and compute hash for city name
         while (chunkPtr[csvSeparatorPos] != CSV_SEPARATOR) {
            FastCharacterHash16Func()(chunkPtr[csvSeparatorPos], h);
            ++csvSeparatorPos;
         }
         std::string_view cityView(chunkPtr, csvSeparatorPos);
         
         // Mininal offset from the csv delimeter. Example: ";x.y", ";-x.y", ";xx.y", ";-xx.y".
         size_t const offsetDotPos = csvSeparatorPos + size_t(2);
         size_t const dotPos = chunkView.find(DECIMAL_SIGN, offsetDotPos);
         size_t const floatNumberStartPos = csvSeparatorPos + size_t(1);
         
         // Floating point number view
         const char* numberPtr = chunkPtr + floatNumberStartPos;
         intermediateResults[chunkIdx].insert_or_assign(h, cityView, parseDecimalNumber_(numberPtr));

         size_t const offsetLineEndPos = dotPos + size_t(2);
         size_t const lineEndPos = chunkView.find('\n', offsetLineEndPos);
         size_t const removeOuterPrefix = (lineEndPos == std::string_view::npos) ? chunkView.size() : lineEndPos + size_t(1);
         chunkView.remove_prefix(removeOuterPrefix);
      }
   });

   for (auto& umap : intermediateResults) {
      for (auto const& [cityView, temps] : umap) {
         result.addTemperature(cityView, temps._min, temps._max, temps._sum, temps._count);
      }
   }   
}


