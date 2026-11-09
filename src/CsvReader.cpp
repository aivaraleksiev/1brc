// Author: Ayvar Aleksiev 2024-2025

#include <algorithm>
#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include "CsvReader.h"
#include "WeatherStation.h"
#include "WeatherHashMap.h"

namespace {
   // CSV delimeter
   constexpr char CSV_SEPARATOR = ';';

   // Size of a single work unit handed out to the worker threads.
   // Threads grab chunks dynamically, so faster cores (P-cores) simply process more chunks
   // than slower ones (E/LP-E cores) instead of waiting for them.
   // Tuned value for AMD Ryzen AI 7 350: 16 MiB.
   constexpr uint64_t CHUNK_SIZE_BYTES = 16ull * 1024 * 1024;
};

struct MemoryChunk {
   std::string_view _chunkView{};
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

   std::vector<MemoryChunk> chunks = splitFileToMemoryChunks_(view, CHUNK_SIZE_BYTES);

   _threadsCount = std::max<uint64_t>(1, std::thread::hardware_concurrency());
   _threadsCount = std::max<uint64_t>(1, std::min<uint64_t>(_threadsCount, chunks.size()));

   processMemoryChunks_(std::move(chunks), result);
   return true;
}

std::vector<MemoryChunk>
CsvReader::splitFileToMemoryChunks_(std::string_view& view, uint64_t chunkSize)
{
   std::vector<MemoryChunk> chunksResult;
   chunksResult.reserve(view.size() / chunkSize + 1);

   size_t start = 0;
   while (start < view.size()) {
      size_t end = std::min(start + chunkSize, view.size());
      // Find the next newline after the proposed end
      while (end < view.size() && view[end] != '\n') {
         ++end;
      }
      if (end < view.size()) {
         ++end; // Include the newline
      }

      chunksResult.emplace_back(std::string_view{ &view[start], end - start });
      start = end;
   }

   return chunksResult;
}

void
CsvReader::processMemoryChunks_(
   std::vector<MemoryChunk>&& fileChunks, WeatherStation& result)
{
   // One hash map per thread, so the hot loop needs no synchronization.
   std::vector<WeatherHashMap> intermediateResults(_threadsCount);

   // Index of the next chunk to be processed. Shared by all worker threads.
   std::atomic<size_t> nextChunk{ 0 };

   auto processChunk = [this](MemoryChunk const& chunk, WeatherHashMap& map) {
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
         map.insert_or_assign(h, cityView, parseDecimalNumber_(cur));
         // Advance the pointer to the character after '\n' or at `end` EOF.
         ++cur;
      }
   };

   // Each worker keeps grabbing the next unprocessed chunk until none are left.
   auto worker = [&](WeatherHashMap& map) {
      for (size_t i; (i = nextChunk.fetch_add(1, std::memory_order_relaxed)) < fileChunks.size();) {
         processChunk(fileChunks[i], map);
      }
   };

   {
      // The calling thread works too, so only (_threadsCount - 1) extra threads are spawned.
      std::vector<std::jthread> threads;
      threads.reserve(_threadsCount - 1);
      for (uint64_t t = 1; t < _threadsCount; ++t) {
         threads.emplace_back(worker, std::ref(intermediateResults[t]));
      }
      worker(intermediateResults[0]);
   }

   for (auto& umap : intermediateResults) {
      for (auto const& [cityView, temps] : umap) {
         result.addTemperature(cityView, temps._min, temps._max, temps._sum, temps._count);
      }
   }
}
