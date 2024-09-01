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
CsvReader::splitFileToMemoryChunks_(std::string_view& view, uint64_t chunkSizePerThread)
{
   std::vector<MemoryChunk> chunksResult;
   chunksResult.reserve(_threadsCount);
   
   size_t runner = 0;
   std::string_view::size_type begin_idx = chunkSizePerThread;
   std::string_view::size_type prevIdx = 0;
   for (uint64_t chunkIdx = 0; chunkIdx < _threadsCount; ++chunkIdx) {
      const auto nextIdx = view.find('\n', begin_idx);
      if (nextIdx != std::string_view::npos) {
         chunksResult.push_back(
            {
             std::string_view { &view[runner], nextIdx - prevIdx + 1}, // +1 is to insert the '\n'
             chunkIdx
            });
         runner += nextIdx - prevIdx + 1;
         prevIdx = nextIdx + 1;
         begin_idx = nextIdx + chunkSizePerThread;
      }
      else {
         chunksResult.push_back(
            {
             std::string_view { &view[runner], view.size() - prevIdx},
             chunkIdx
            });
         break;
      }
   }

   return chunksResult;
}

void
CsvReader::processMemoryChunks_(
   std::vector<MemoryChunk>&& fileChunks, WeatherStation& result)
{
   std::vector<WeatherHashMap> intermediateResults(_threadsCount);

   std::for_each(std::execution::par_unseq, fileChunks.begin(), fileChunks.end(), [&](struct MemoryChunk const& chunk) {

      auto chunkWiew = chunk._chunkView;
      auto const chunkIdx = chunk._idx;

      while (!chunkWiew.empty()) {
         //
         const char* chunkPtr = chunkWiew.data();
         hash_t h = 0;
         size_t csvSeparatorPos = 0;
         while (chunkPtr[csvSeparatorPos] != CSV_SEPARATOR) {
            FastCharacterHash16Func()(chunkPtr[csvSeparatorPos], h);
            ++csvSeparatorPos;
         }

         // Mininal offset from the csv delimeter. Example: ";x.y", ";-x.y", ";xx.y", ";-xx.y".
         size_t const offsetDotPos = csvSeparatorPos + size_t(2);
         size_t const dotPos = chunkWiew.find(DECIMAL_SIGN, offsetDotPos);
         size_t const floatNumberStartPos = csvSeparatorPos + size_t(1);
         auto const floatNumberLength = dotPos + size_t(1) - csvSeparatorPos; // Example: ...h;13.5

         std::string_view cityView = chunkWiew.substr(0, csvSeparatorPos);
         std::string_view numberView = chunkWiew.substr(floatNumberStartPos, floatNumberLength);

         intermediateResults[chunkIdx].insert_or_assign(h, cityView, parseDecimalNumber(numberView.data()));

         size_t const offsetLineEndPos = dotPos + size_t(2);
         size_t const lineEndPos = chunkWiew.find('\n', offsetLineEndPos);
         size_t const removeOuterPrefix = (lineEndPos == std::string_view::npos) ? chunkWiew.size() : lineEndPos + size_t(1);
         chunkWiew.remove_prefix(removeOuterPrefix);
      }
   });

   for (auto& umap : intermediateResults) {
      for (auto const& [cityView, temps] : umap) {
         result.addTemperature(cityView, temps._min, temps._max, temps._sum, temps._count);
      }
   }   
}


