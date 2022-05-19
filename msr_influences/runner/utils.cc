#include "utils.h"

#include <sys/types.h>
#include <pthread.h>

#include <algorithm>
#include <sstream>


std::vector<std::string> SplitString(const std::string& input_str, char delimiter) {
  std::string delims;
  delims += delimiter;
  std::vector<std::string> results;

  // pre-reserve for performance reasons
  results.reserve(2);

  std::string::const_iterator start = input_str.begin();
  std::string::const_iterator end = input_str.end();
  std::string::const_iterator next = std::find(start, end, delimiter);
  while (next != end) {
    results.emplace_back(start, next);
    start = next + 1;
    next = std::find(start, end, delimiter);
  }
  results.emplace_back(start, next);
  return results;
}

std::string ToHex(uint64_t number) {
  std::stringstream ss;
  ss << std::hex << "0x" << number;
  return ss.str();
}

int Popcount(uint64_t number) {
  int count = 0;
  while (number != 0) {
    count += static_cast<int>(number & 1ull);
    number >>= 1u;
  }
  return count;
}

void PrintInfoMessage(const std::string& message) {
  std::stringstream msg_stream;
  // change color
  msg_stream << "\033[33m";
  //AddTimestamp(msg_stream);
  msg_stream << "[*] " << message;
  // change color back to normal
  msg_stream << "\033[0m";
  std::cout << msg_stream.str() << std::endl;
}

void PrintSuccessMessage(const std::string& message) {
  std::stringstream msg_stream;
  // change color
  msg_stream << "\033[92m";
  //AddTimestamp(msg_stream);
  msg_stream << "[+] " << message;
  // change color back to normal
  msg_stream << "\033[0m";
  std::cout << msg_stream.str() << std::endl;
}

void PrintWarningMessage(const std::string& message) {
  std::stringstream msg_stream;
  // change color
  msg_stream << "\033[93m";
  //AddTimestamp(msg_stream);
  msg_stream << "[+] " << message;
  // change color back to normal
  msg_stream << "\033[0m";
  std::cout << msg_stream.str() << std::endl;
}

void PrintErrorMessage(const std::string& message) {
  std::stringstream msg_stream;
  // change color
  msg_stream << "\033[31m";
  //AddTimestamp(msg_stream);
  msg_stream << "[-] " << message;
  // change color back to normal
  msg_stream << "\033[0m";
  std::cerr << msg_stream.str() << std::endl;
}
