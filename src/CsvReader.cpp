// Author: Ayvar Aleksiev 2024-2025

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstring>
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

   // The SWAR fast path loads 8 bytes at a time and may read past the end of a row.
   // A row is at most 100 (name) + 1 (';') + 5 ("-99.9") + 1 ('\n') = 107 bytes, plus up to
   // 8 bytes of over-read. Rows starting within the last kFastPathMargin bytes of the file are
   // parsed with the byte-by-byte path, so no read ever goes past the end of the mapping.
   constexpr std::ptrdiff_t kFastPathMargin = 128;

   // The SWAR tricks below assume the first byte of a loaded word is its lowest byte.
   static_assert(std::endian::native == std::endian::little, "SWAR parsing requires little-endian");

   // kLowBytesMask[n] keeps the lowest n bytes of a word (n = 0..8).
   constexpr uint64_t kLowBytesMask[9] = {
      0x0000000000000000ull, 0x00000000000000FFull, 0x000000000000FFFFull,
      0x0000000000FFFFFFull, 0x00000000FFFFFFFFull, 0x000000FFFFFFFFFFull,
      0x0000FFFFFFFFFFFFull, 0x00FFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull,
   };

   // kWordIsFull[n] is all ones only if n == 8, i.e. the first word contained no ';'
   // and the name continues into the second word.
   constexpr uint64_t kWordIsFull[9] = { 0, 0, 0, 0, 0, 0, 0, 0, ~0ull };

   ALWAYS_INLINE uint64_t load64(const char* p)
   {
      uint64_t word;
      std::memcpy(&word, p, sizeof(word));
      return word;
   }

   // SWAR: returns a word with bit 7 of each ';' byte set (0 if the word has no ';').
   // XOR turns ';' bytes into 0x00, then the classic "has zero byte" trick flags them.
   // Bits above the first match may be false positives, but the LOWEST set bit is always the
   // first ';', which is all that std::countr_zero looks at.
   ALWAYS_INLINE uint64_t findSeparator(uint64_t word)
   {
      const uint64_t x = word ^ 0x3B3B3B3B3B3B3B3Bull; // 0x3B == ';'
      return (x - 0x0101010101010101ull) & ~x & 0x8080808080808080ull;
   }
};

struct MemoryChunk {
   std::string_view _chunkView{};
};

bool
CsvReader::parse(std::string const& file, WeatherStation& result)
{
   try {
      _csvFileRead.open(file);
      if (!_csvFileRead.is_open()) {
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

   std::string_view view(_csvFileRead.data(), _csvFileRead.size());

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
   if (fileChunks.empty()) {
      return;
   }

   // One hash map per thread, so the hot loop needs no synchronization.
   std::vector<WeatherHashMap> intermediateResults(_threadsCount);

   // Index of the next chunk to be processed. Shared by all worker threads.
   std::atomic<size_t> nextChunk{ 0 };

   // Rows starting before `fastPathEnd` can safely use 8-byte over-reading loads.
   const char* fileBegin = fileChunks.front()._chunkView.data();
   const char* fileEnd = fileChunks.back()._chunkView.data() + fileChunks.back()._chunkView.size();
   const char* fastPathEnd =
      (fileEnd - fileBegin > kFastPathMargin) ? fileEnd - kFastPathMargin : fileBegin;

   auto processChunk = [this, fastPathEnd](MemoryChunk const& chunk, WeatherHashMap& map) {
      const char* cur = chunk._chunkView.data();
      const char* end = cur + chunk._chunkView.size();
      const char* fastEnd = std::min(end, fastPathEnd);

      // Fast path: SWAR name scanning (8 bytes at a time) + branchless number parsing.
      while (cur < fastEnd) {
         const char* cityNamePtr = cur; // marks start of city name

         // Load the first 16 bytes of the row and look for ';' in both words at once.
         uint64_t w0 = load64(cur);
         uint64_t w1 = load64(cur + 8);
         const uint64_t sep0 = findSeparator(w0);
         const uint64_t sep1 = findSeparator(w1);

         // Name bytes in each word (8 == no ';' in that word).
         const int len0 = std::countr_zero(sep0) >> 3;
         const int len1 = std::countr_zero(sep1) >> 3;

         // Zero everything from the ';' onward. w1 only belongs to the name if w0 had no ';'.
         const uint64_t w1Used = kWordIsFull[len0];
         w0 &= kLowBytesMask[len0];
         w1 &= w1Used & kLowBytesMask[len1];

         const char* separator;
         if ((sep0 | sep1) != 0) {
            // Name is at most 15 bytes (the vast majority of rows): no loop at all.
            separator = cur + len0 + (len1 & static_cast<int>(w1Used));
         }
         else {
            // Long name (16+ bytes): keep scanning 8 bytes at a time.
            const char* p = cur + 16;
            uint64_t sep;
            while ((sep = findSeparator(load64(p))) == 0) {
               p += 8;
            }
            separator = p + (std::countr_zero(sep) >> 3);
         }

         std::string_view cityView(cityNamePtr, separator - cityNamePtr);

         // Skip ';', parse the number and move to the start of the next line.
         cur = separator + 1;
         const int temperature = parseDecimalNumberBranchless_(cur);
         map.insert_or_assign(HashNameWords(w0, w1), w0, w1, cityView, temperature);
      }

      // Slow path for the last rows of the file: byte by byte, never reads past `end`.
      // Produces the same w0/w1/hash as the fast path, so both paths share hash map slots.
      while (cur < end) {
         const char* cityNamePtr = cur; // marks start of city name
         while (*cur != CSV_SEPARATOR) {
            ++cur;
         }
         const size_t nameLen = static_cast<size_t>(cur - cityNamePtr);

         uint64_t w0 = 0;
         uint64_t w1 = 0;
         std::memcpy(&w0, cityNamePtr, std::min<size_t>(nameLen, 8));
         if (nameLen > 8) {
            std::memcpy(&w1, cityNamePtr + 8, std::min<size_t>(nameLen - 8, 8));
         }
         std::string_view cityView(cityNamePtr, nameLen);

         // skip csv separator ';' and move to floating point number starting position.
         ++cur;
         map.insert_or_assign(HashNameWords(w0, w1), w0, w1, cityView, parseDecimalNumber_(cur));
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
      for (auto const& entry : umap) {
         auto const& temps = entry.stats;
         result.addTemperature(entry.name, temps._min, temps._max, temps._sum, temps._count);
      }
   }
}
