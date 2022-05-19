#ifndef MSR_UOPS_RUNNERV2_UTILS_H_
#define MSR_UOPS_RUNNERV2_UTILS_H_

#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

void PrintInfoMessage(const std::string& message);
void PrintSuccessMessage(const std::string& message);
void PrintWarningMessage(const std::string& message);
void PrintErrorMessage(const std::string& message);

std::vector<std::string> SplitString(const std::string& input_str, char delimiter);
std::string ToHex(uint64_t number);

int Popcount(uint64_t number);

/// Calculates the median of a given vector
/// \tparam T type of the vector elements
/// \param values list of values
/// \return median
template<class T>
double median(std::vector<T> values) {
  if (values.empty()) {
    return 0;
  }
  std::sort(values.begin(), values.end());
  if (values.size() % 2 == 0) {
    return static_cast<double>((values[(values.size() - 1) / 2] + values[values.size() / 2])) / 2;
  } else {
    return values[values.size() / 2];
  }
}

#endif // MSR_UOPS_RUNNERV2_UTILS_H_
