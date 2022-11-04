#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

void logToCSV(std::ofstream& csv_file, std::vector<std::string> values)
{
   size_t i = 0;
   for (; i < values.size() - 1; i++) {
      csv_file << values[i] << ",";
   }
   csv_file << values[i] << std::endl;
}

void openCSV(std::ofstream& csv_file, std::string csv_file_path, std::vector<std::string> header)
{
   std::ofstream::openmode open_flags = std::ios::app;
   bool csv_initialized = std::filesystem::exists(csv_file_path);

   csv_file.open(csv_file_path, open_flags);
   if (!csv_initialized) {
      // print header
      logToCSV(csv_file, header);
   }
}
