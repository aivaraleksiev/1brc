// Author: Ayvar Aleksiev 2024-2025

#ifdef _WIN32
// Necessary for SetConsoleOutputCP and CP_UTF8
#define NOMINMAX
#include <windows.h>
#endif

#include <chrono>
#include <print>

#include "WeatherStation.h"
#include "CsvReader.h"



int main(int argc, char **argv)
{
   if (argc < 2) {
      std::println(stderr, "Usage: 1brc <file>");
      return 1;
   }
#ifdef _WIN32
   // Set the console output code page to UTF-8
   SetConsoleOutputCP(CP_UTF8);
#endif

   auto startTime = std::chrono::steady_clock::now();

   CsvReader reader;
   WeatherStation station;
   reader.parse(argv[1], station);
   station.print();

   auto endTime = std::chrono::steady_clock::now();
   std::chrono::duration<double> const diffTime = endTime - startTime;

   std::print("\n----------\nTime 1brc: {:.9f}\n----------\n", diffTime.count());
}
