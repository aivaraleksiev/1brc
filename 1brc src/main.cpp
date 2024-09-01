// Author: Ayvar Aleksiev 2024

#ifdef WIN32
// Necessary for SetConsoleOutputCP and CP_UTF8
#include <windows.h> 
#endif

#include <iostream>
#include <chrono>

#include "WeatherStation.h"
#include "CsvReader.h"



int main(int argc, char **argv)
{
#ifdef WIN32
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
   
   std::cout << "\n----------\n"
             << "Time 1brc: " << std::fixed << std::setprecision(9) << diffTime.count()
             << "\n----------\n";
}