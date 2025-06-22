// Author: Ayvar Aleksiev 2024

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <cstdio>

using CityNameView = std::string_view;

inline constexpr int32_t InvalidTempForMin= 200;
inline constexpr int32_t InvalidTempForMax = -200;

// Empirically determined upper bound for the largest printable payload observed (~10,666 chars).
// 167 cache lines × 64 bytes (typical x86_64 cache line size) = 10,688 bytes. Adjust if needed.
// Used for buffer sizing to avoid dynamic allocation in the common case. 
inline constexpr std::size_t kMaxOutputBytes = 10688;

// Work buffer max size for one formatted entry, e.g., "=-56.0/-21.2/-72.6, "
// Maximum 20 characters + trailing NUL.
inline constexpr std::size_t kStatsBufferSize = 21;

class WeatherStation
{
public:

   struct TempStats {
      inline void addTemp(int32_t temp)
      {
         if (temp < _min) {
            _min = temp;
         }
         if (temp > _max) {
            _max = temp;
         }
         _sum += temp;
         ++_count;
      }

      int32_t _min { InvalidTempForMin };
      int32_t _max { InvalidTempForMax }; 
      int32_t _sum { 0 };
      int32_t _count{ 0 };
   };

   inline void addTemperature(CityNameView city, int32_t min, int32_t max, int32_t sum, int32_t size)
   {   
      auto& stats = _cityTemps[city];
      if (stats._min > min) {
         stats._min = min;
      }
      if (stats._max < max) {
         stats._max = max;
      }
      stats._sum += sum;
      stats._count += size;
   }

   void print() const {
      std::string result;
      result.reserve(kMaxOutputBytes);
      result.push_back('{');

      char statsBuffer[kStatsBufferSize];

      for (const auto& [cityView, stat] : _cityTemps)
      {
         const float min = 0.1f * stat._min;
         const float avg = 0.1f * static_cast<float>(stat._sum) / stat._count;
         const float max = 0.1f * stat._max;

         const int wBytesLen =
            std::snprintf(statsBuffer, sizeof(statsBuffer), "=%.1f/%.1f/%.1f, ", min, avg, max);

         result.append(cityView);
         result.append(statsBuffer, static_cast<std::size_t>(wBytesLen));
      }

      result.append("}\n", 2);
      std::fwrite(result.data(), 1, result.size(), stdout);
   }

   private:
   std::map<CityNameView, TempStats> _cityTemps;
};
