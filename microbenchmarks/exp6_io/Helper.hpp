#pragma once

#include <cstring>
#include <atomic>
#include <cassert>
#include <string> 
#include <iostream>
#include <fstream>


namespace csv {

template <typename output, typename Arg>
void write_csv(output& out, Arg arg) {
   out << std::to_string(arg);
}

template <typename output>
void write_csv(output& out, std::string arg) {
   out << arg;
}

template <typename output>
void write_csv(output& out, const char*& arg) {
   out << arg;
}

template <typename output, typename First, typename... Args>
void write_csv(output &out, First first, Args... args) {
   write_csv( out, first);
   write_csv( out, std::string(","));
   if constexpr (sizeof...(Args) == 1) {
         write_csv(out, args...);
         write_csv(out, std::string("\n"));
      }else{
      write_csv(out, args...);
   }
}

}  // csv
