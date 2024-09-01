// Author: Ayvar Aleksiev 2024

#pragma once

#include <map>
#include <string_view>
#include <iostream>
#include <sstream>

using CityNameView = std::string_view;

inline constexpr int32_t InvalidTempForMin= 200;
inline constexpr int32_t InvalidTempForMax = -200;

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

   inline void addTemperature(
      const CityNameView& city,
      int32_t min, int32_t max, int32_t sum, int32_t size) {
      
      if (_cityTemps[city]._min > min) {
         _cityTemps[city]._min = min;
      }
      if (_cityTemps[city]._max < max) {
         _cityTemps[city]._max = max;
      }
      _cityTemps[city]._sum += sum;
      _cityTemps[city]._count += size;


   }

   void print() const {
      std::stringstream ostr;
      std::cout << '{';
      for (auto const& [city, tmpStats] : _cityTemps) {
         ostr << city << "=" << tmpStats._min * 0.1;
         ostr << std::fixed << std::setprecision(1);
         ostr << "/" << static_cast<float>(tmpStats._sum * 0.1 ) / tmpStats._count;
         ostr << "/";
         ostr << tmpStats._max * 0.1;
         ostr << ", ";
      }
      ostr << "}\n";
      std::cout << ostr.str();
   }

   private:
   std::map<CityNameView, TempStats> _cityTemps;
};
