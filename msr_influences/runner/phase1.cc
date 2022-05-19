#include "phase1.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
#include <fstream>

std::string ErrorCodeToString(ErrorCode error_code) {
  switch (error_code) {
    case ErrorCode::kSuccess:
      return "Success";
    case ErrorCode::kInitialReadError:
      return "InitialReadError";
    case ErrorCode::kReadAfterFailedWriteError:
      return "ReadAfterFailedWriteError";
    case ErrorCode::kReadAfterSuccessfulWriteError:
      return "ReadAfterSuccessfulWriteError";
    case ErrorCode::kRestoreError:
      return "RestoreError";
    case ErrorCode::kValueChangeAfterFailedWriteError:
      return "ValueChangeAfterFailedWriteError";
  }
  // unreachable
  std::abort();
}

ErrorCode StringToErrorCode(const std::string& error_code) {
  if (error_code == "Success") {
    return ErrorCode::kSuccess;
  } else if (error_code == "InitialReadError") {
    return ErrorCode::kInitialReadError;
  } else if (error_code == "ReadAfterFailedWriteError") {
    return ErrorCode::kReadAfterFailedWriteError;
  } else if (error_code == "ReadAfterSuccessfulWriteError") {
    return ErrorCode::kReadAfterSuccessfulWriteError;
  } else if (error_code == "RestoreError") {
    return ErrorCode::kRestoreError;
  } else if (error_code == "ValueChangeAfterFailedWriteError") {
    return ErrorCode::kValueChangeAfterFailedWriteError;
  } else {
    PrintErrorMessage("The string '"
                          + error_code + "' is not a viable ErrorCode representation");
    std::abort();
  }
}

void PrintStatisticsForWritableBits(const Phase1Results& results) {
  int writable_bits_sum = 0;
  std::vector<int> writable_bits;
  uint64_t exhaustive_search_space = 0;
  uint64_t actual_search_space = 0;
  uint64_t writable_msrs = 0;
  for (Phase1Result result : results) {
    int bits = Popcount(result.flip_bitmask);
    if (bits) { writable_msrs++; }
    writable_bits.emplace_back(bits);
    writable_bits_sum += bits;
    exhaustive_search_space += std::pow(2, bits);
    actual_search_space += std::pow(2, kBitWindowSize) * bits;
  }

  int bits_avg, bits_min, bits_max;
  double bits_median;
  if (!writable_bits.empty()) {
    bits_avg = writable_bits_sum / writable_bits.size();
    bits_min = *std::min_element(writable_bits.begin(), writable_bits.end());
    bits_max = *std::max_element(writable_bits.begin(), writable_bits.end());
    bits_median = median<int>(writable_bits);
  } else {
    bits_avg = 0;
    bits_min = 0;
    bits_max = 0;
    bits_median = 0;
  }
  if (!writable_bits.empty()) {
  }

  PrintInfoMessage("Found " + std::to_string(writable_bits_sum) + " writable bits in "
                       + std::to_string(writable_msrs) + " MSRs. " +
                       "Tested " + std::to_string(results.size()) + " MSRs.");
  PrintInfoMessage("Bits per MSR: " + std::to_string(bits_avg) + " (average)");
  PrintInfoMessage("Bits per MSR: " + std::to_string(bits_median) + " (median)");
  PrintInfoMessage("Bits per MSR: " + std::to_string(bits_min) + " (min)");
  PrintInfoMessage("Bits per MSR: " + std::to_string(bits_max) + " (max)");
  PrintInfoMessage("Exhaustive search space: " + std::to_string(exhaustive_search_space)
                       + " values");
  PrintInfoMessage("Actual search space: " + std::to_string(actual_search_space)
                       + " values");

}

ErrorCode CheckFlippableBits(const msr_context& msr,
                             const MSRInterface& msr_interface,
                             bool keep_write_only_msrs,
                             uint64_t* flippable_bits) {
  uint64_t original_value;
  *flippable_bits = 0;
  int err = msr_interface.Read(msr.address, &original_value);
  if (err && !keep_write_only_msrs) {
    return ErrorCode::kInitialReadError;
  }
  if (err) {
    // in case of write-only MSRs we assume the MSR is initially 0
    original_value = 0;
  }

  for (int bit_pos : msr.bits_to_check) {
    uint64_t modified_value = original_value ^(((uint64_t) 1) << bit_pos);
    PrintInfoMessage("Writing " + ToHex(modified_value) + " to MSR " + ToHex(msr.address));
    err = msr_interface.Write(msr.address, modified_value);
    if (err) {
      // write failed
      uint64_t after_fail_value;
      err = msr_interface.Read(msr.address, &after_fail_value);
      if (err) {
        continue;
        //return ErrorCode::kReadAfterFailedWriteError;
      }
      if (original_value != after_fail_value) {
        continue;
        //return ErrorCode::kValueChangeAfterFailedWriteError;
      }
    } else {
      // write did not fault
      uint64_t after_write_value;
      err = msr_interface.Read(msr.address, &after_write_value);
      if (err && !keep_write_only_msrs) {
        continue;
        //return ErrorCode::kReadAfterSuccessfulWriteError;
      }

      if (after_write_value == modified_value || (err && keep_write_only_msrs)) {
        // write successful
        err = msr_interface.Write(msr.address, original_value);
        if (err) {
          return ErrorCode::kRestoreError;
        }

        *flippable_bits ^= (((uint64_t) 1) << bit_pos);
      }
    }
  }
  return ErrorCode::kSuccess;
}

Phase1Results SearchFlippableBits(const std::vector<msr_context>& msrs,
                                  uint32_t core_id,
                                  bool keep_write_only_msrs) {
  Phase1Results results;
  MSRInterface msr_interface(core_id);
  for (msr_context msr : msrs) {
    std::cout << std::hex << "[*] checking MSR " << ToHex(msr.address) << std::endl;

    // excluded msrs TODO: clean up
    if (msr.address == 0xc0000101 || // cpu freeze
        msr.address == 0x1b || // cpu freeze
        msr.address == 0x2e0 || // cpu freeze
        msr.address == 0xc0000080 || // os restart
        msr.address == 0x1a0         // os restart
        ) {
      PrintWarningMessage("Skipping MSR because it breaks things.");
      continue;
    }
    uint64_t flip_bitmask;
    ErrorCode ec = CheckFlippableBits(msr, msr_interface, keep_write_only_msrs, &flip_bitmask);
    results.emplace_back(msr.address, ec, flip_bitmask);
  }
  return results;
}

void WriteOutFlippableBits(const std::string& filename, const Phase1Results& flip_results) {
  std::ofstream fd(filename);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open" + filename + "! Aborting!");
    exit(1);
  }
  std::string headerline = "msr;errorcode;flipmask";
  fd << headerline << std::endl;
  for (auto& result : flip_results) {
    uint32_t msr = result.msr;
    uint64_t flipmask = result.flip_bitmask;
    std::string err_code = ErrorCodeToString(result.error_code);
    if (result.error_code != ErrorCode::kSuccess) {
      flipmask = 0;
    }
    fd << ToHex(msr) << ";" << err_code << ";" << ToHex(flipmask) << std::endl;
  }
}

Phase1Results ReadInFlippableBits(const std::string& filename) {
  std::ifstream fd(filename);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open " + filename + "! Aborting! Try running --phase1");
    exit(1);
  }

  std::string headerline = "msr;errorcode;flipmask";
  std::string line;
  std::getline(fd, line);
  if (line != headerline) {
    PrintErrorMessage("File " + filename + " has an incorrect format");
    exit(1);
  }

  Phase1Results results;
  while (std::getline(fd, line)) {
    std::vector<std::string> line_splitted = SplitString(line, ';');
    if (line_splitted.size() != 3) {
      PrintErrorMessage("[-] Could not parse line of " + filename + ". Aborting!");
      exit(1);
    }
    uint32_t msr = std::stoull(line_splitted[0], 0, 16);
    ErrorCode ec = StringToErrorCode(line_splitted[1]);
    uint64_t flip_bitmask = std::stoull(line_splitted[2], 0, 16);
    results.emplace_back(msr, ec, flip_bitmask);
  }
  return results;
}
