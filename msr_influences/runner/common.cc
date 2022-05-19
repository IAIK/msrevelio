#include "common.h"

#include <cstdint>
#include <cmath>
#include <fstream>
#include <unordered_map>

//
// external constants
//
const std::string kInstructionFolder = "../../data/instructions";
const int kBitWindowSize = 4;
//
// internal constants
//
const std::string kPathToNanobenchScript = "../../scripts/invoke-nanobench_combined_dir-v2.sh";
constexpr double kUpperThresholdDifference = 1.1;  // 20% more than reference
constexpr double kLowerThresholdDifference = 0.9;  // 20% less than reference
constexpr double kMinimalAbsoluteDifference = 1.0;
constexpr int kConsecutiveRunsForCalibration = 15;
constexpr int kMSRMaxBitSize = 64;
static_assert(kUpperThresholdDifference > 1.0, "Upper threshold has illegal value!");
static_assert(kLowerThresholdDifference < 1.0, "Lower threshold has illegal value!");

int CountBitsToCheck(const std::vector<msr_context>& msrs) {
  int bits = 0;
  for (const msr_context& msr : msrs) {
    bits += msr.bits_to_check.size();
  }
  return bits;
}

void SetCPUAffinity(int cpu) {
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  int s = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  if (s != 0) {
    fprintf(stderr, "could not set cpu affinity!\n");
    exit(-1);
  }
}

int FlipBits(uint32_t msr,
             uint64_t bitmask_to_flip,
             bool restore_old_value,
             bool accept_write_only_msrs,
             const MSRInterface& msr_interface) {

  uint64_t old_value;
  int err = msr_interface.Read(msr, &old_value);
  if (err && !accept_write_only_msrs) {
    PrintWarningMessage("Could not read value of MSR " + ToHex(msr) + ".");
    return 1;
  }
  if (err && !restore_old_value) {
    // in case of write-only MSRs we assume the MSR is initially 0
    old_value = 0;
  }
  if (err && restore_old_value) {
    // in case of write-only MSRs we need to flip back the value (flip back iff restore)
    old_value = bitmask_to_flip;
  }
  uint64_t new_value = old_value ^bitmask_to_flip;
  PrintInfoMessage("Writing value " + ToHex(new_value) + " to MSR " + ToHex(msr));
  err = msr_interface.Write(msr, new_value);
  if (err) {
    PrintWarningMessage("Could not write value of MSR " + ToHex(msr) + ".");
    return 2;
  }
  return 0;
}

int RecordPMCs(uint32_t core_id, const std::string& instruction_folder,
               const std::string& output_file) {
  std::string cmd = kPathToNanobenchScript;
  cmd += " ";
  cmd += std::to_string(core_id);
  cmd += " ";
  cmd += instruction_folder;
  cmd += " ";
  cmd += output_file;
  int ret = system(cmd.c_str());
  if (ret != 0) {
    PrintErrorMessage("Failed executing command '" + cmd + "'");
    return 1;
  }
  return 0;
}

std::string CreatePMCReferenceThresholdMapKey(const std::string& testname,
                                              const std::string& pmcname) {
  std::string key = testname;
  key += "--";
  key += pmcname;
  return key;
}

std::string CreatePMCReferenceThresholdMapKey(const PMCReferenceThreshold& threshold) {
  return CreatePMCReferenceThresholdMapKey(threshold.test_name, threshold.pmc_name);
}

PMCReferenceThresholdMap CreatePMCReferenceThresholds(
    uint32_t core_id,
    const std::string& instruction_folder) {
  std::string reference_output("TODO_reference_output");
  int err = RecordPMCs(core_id, instruction_folder, reference_output);
  if (err) {
    PrintErrorMessage("Error recording reference state. Aborting!");
    exit(1);
  }
  std::unordered_map<std::string, PMCReferenceThreshold> reference_thresholds;

  std::ifstream fd(reference_output);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open " + reference_output + ". Aborting!");
    exit(1);
  }

  std::string line;
  std::getline(fd, line);
  std::vector<std::string> header = SplitString(line, ';');
  if (header[0] != "NAME") {
    PrintErrorMessage("Invalid file format of nanoBench result. Aborting!");
    exit(1);
  }

  while (std::getline(fd, line)) {
    std::vector<std::string> splitted_line = SplitString(line, ';');
    std::string testname = splitted_line[0];
    for (size_t i = 1; i < splitted_line.size(); i++) {
      if (splitted_line[i].empty()) {
        PrintErrorMessage(
            "NanoBench result is missing or has improper formatting. Aborting!");
        exit(1);
      }
      std::string pmc_name = header[i];
      std::string key = CreatePMCReferenceThresholdMapKey(testname, pmc_name);
      double reference_value = std::stod(splitted_line[i]);
      double upper_threshold = reference_value;// * kUpperThresholdDifference;
      double lower_threshold = reference_value;// * kLowerThresholdDifference;
      //if (reference_value + kMinimalAbsoluteDifference > upper_threshold) {
      //  upper_threshold = reference_value + kMinimalAbsoluteDifference;
      //}
      //if (reference_value - kMinimalAbsoluteDifference < lower_threshold) {
      //  lower_threshold = reference_value - kMinimalAbsoluteDifference;
      //}
      PMCReferenceThreshold pmc_reference_threshold(testname,
                                                    pmc_name,
                                                    reference_value,
                                                    lower_threshold,
                                                    upper_threshold);
      reference_thresholds[key] = pmc_reference_threshold;
    }
  }
  return reference_thresholds;
}

void WriteOutPMCThresholds(const std::string& filename,
                           const PMCReferenceThresholdMap& pmc_reference_thresholds) {
  std::ofstream fd(filename);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open" + filename + "! Aborting!");
    exit(1);
  }
  std::string headerline = PMCReferenceThreshold::GetCSVHeaderline();
  fd << headerline << std::endl;
  for (const auto& key_with_threshold : pmc_reference_thresholds) {
    PMCReferenceThreshold threshold = key_with_threshold.second;
    fd << threshold.ToCSV() << std::endl;
  }
}

PMCReferenceThresholdMap ReadInPMCThresholds(const std::string& filename) {
  std::ifstream fd(filename);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open " + filename + "! Aborting! "
                                                     "Try running --calibrate");
    exit(1);
  }
  std::string headerline = PMCReferenceThreshold::GetCSVHeaderline();
  std::string line;
  std::getline(fd, line);
  if (line != headerline) {
    PrintErrorMessage("File " + filename + " has an incorrect format");
    exit(1);
  }
  PMCReferenceThresholdMap pmc_reference_threshold_map;
  while (std::getline(fd, line)) {
    PMCReferenceThreshold threshold;
    threshold.LoadFromCSV(line);
    std::string key = CreatePMCReferenceThresholdMapKey(threshold);
    pmc_reference_threshold_map[key] = threshold;
  }
  return pmc_reference_threshold_map;
}

PMCDifferences CheckForPMCDifference(PMCReferenceThresholdMap reference_threshold_map,
                                     const std::string& test_results) {
  std::ifstream fd(test_results);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open " + test_results + ". Aborting!");
    exit(1);
  }

  std::string line;
  std::getline(fd, line);
  std::vector<std::string> header = SplitString(line, ';');
  if (header[0] != "NAME") {
    PrintErrorMessage("Invalid file format of nanoBench result. Aborting!");
    exit(1);
  }

  std::vector<std::pair<PMCReferenceThreshold, double>> results;
  while (std::getline(fd, line)) {
    std::vector<std::string> splitted_line = SplitString(line, ';');
    std::string test_name = splitted_line[0];
    for (size_t i = 1; i < splitted_line.size(); i++) {
      if (splitted_line[i].empty()) {
        PrintErrorMessage("Invalid file format of nanoBench result. Aborting!");
        exit(1);
      }
      double value = std::stod(splitted_line[i]);
      std::string pmc_name = header[i];
      std::string ref_key = CreatePMCReferenceThresholdMapKey(test_name, pmc_name);

      // manually increase thresholds for different PMCs
      // TODO: outline this logic to a separate configuration file
      double minimal_difference = kMinimalAbsoluteDifference;
      if (pmc_name == "RDTSC" || pmc_name.find("Cycles") != std::string::npos
          || pmc_name.find("cycles") != std::string::npos) {
        // atleast 100 cycle diff
        minimal_difference = 100.0;
      }
      if (pmc_name.find("RETIRED") != std::string::npos) {
        // atleast 3 retired loads diff
        minimal_difference = 3.0;
      }

      PMCReferenceThreshold reference_threshold = reference_threshold_map[ref_key];
      if (value < reference_threshold.lower_threshold ||
          value > reference_threshold.upper_threshold) {
        if (std::abs(value - reference_threshold.reference_value) < minimal_difference) {
          // ignore values whose difference is too low
          continue;
        }
        PrintSuccessMessage("Difference in " + reference_threshold.test_name
                                + ":" + reference_threshold.pmc_name);
        PrintSuccessMessage("Observed: " + std::to_string(value)
                                + " (reference: "
                                + std::to_string(reference_threshold.reference_value) + ")");
        results.emplace_back(reference_threshold, value);
      }
    }
  }
  return results;
}

void CalibrateReferenceThresholds(const std::string& pmc_threshold_file,
                                  uint32_t core_id,
                                  uint32_t maximum_calibrate_tries) {
  MSRInterface msr_interface(core_id);
  std::string pmc_test_results("TODO_pmc_calibrate_results");

  PMCReferenceThresholdMap pmc_reference_thresholds =
      CreatePMCReferenceThresholds(core_id, kInstructionFolder);

  int successful_subsequent_runs = 0;
  for (uint32_t i = 0; i < maximum_calibrate_tries &&
      successful_subsequent_runs < kConsecutiveRunsForCalibration; i++) {
    int err = RecordPMCs(core_id, kInstructionFolder, pmc_test_results);
    if (err) {
      PrintErrorMessage("Error recording PMC events. Aborting");
      return;
    }
    std::vector<std::pair<PMCReferenceThreshold, double>> diffs =
        CheckForPMCDifference(pmc_reference_thresholds, pmc_test_results);
    if (diffs.empty()) {
      successful_subsequent_runs += 1;
    } else {
      // we still observed differences hence we adjust thresholds
      successful_subsequent_runs = 0;
      for (const std::pair<PMCReferenceThreshold, double>& diff : diffs) {
        PMCReferenceThreshold threshold = diff.first;
        double detected_value = diff.second;
        if (detected_value > threshold.upper_threshold) {
          double old_value = threshold.upper_threshold;
          double new_value =
              std::max(detected_value, threshold.upper_threshold * kUpperThresholdDifference);
          threshold.upper_threshold = new_value;
          PrintInfoMessage("Updated upper threshold " + threshold.pmc_name
                               + ": " + threshold.test_name
                               + " from " + std::to_string(old_value)
                               + " to " + std::to_string(new_value));
        } else {
          double old_value = threshold.lower_threshold;
          double new_value =
              std::min(detected_value, threshold.lower_threshold * kLowerThresholdDifference);
          if (std::fabs(old_value - new_value) <= std::numeric_limits<double>::epsilon()) {
            // handle special cases such as very low values or the value 0
            new_value = old_value - 1;
          }
          threshold.lower_threshold = new_value;
          PrintInfoMessage("Updated lower threshold " + threshold.pmc_name
                               + ": " + threshold.test_name
                               + " from " + std::to_string(old_value)
                               + " to " + std::to_string(new_value));
        }
        // update thresholds with new values
        std::string key = CreatePMCReferenceThresholdMapKey(threshold);
        pmc_reference_thresholds[key] = threshold;
      }
    }
  }
  if (successful_subsequent_runs != kConsecutiveRunsForCalibration) {
    PrintErrorMessage("Could not finish calibration of PMC thresholds.");
    exit(0);
  }
  PrintSuccessMessage("Successfully calibrated PMC thresholds.");
  WriteOutPMCThresholds(pmc_threshold_file, pmc_reference_thresholds);
}

/// For given bitpositions calculate all values that represent permutations of these bits
/// WARNING: lists can contain duplicates
std::vector<uint64_t> BitPositionsToAllPossibleBitmasks(std::vector<uint64_t> bit_positions) {
  std::vector<uint64_t> bitmasks;
  if (bit_positions.empty()) {
    // return empty list
    return bitmasks;
  }
  if (bit_positions.size() == 1) {
    uint64_t num1 = 1ull << bit_positions[0];
    uint64_t num2 = 0;
    bitmasks.emplace_back(num1);
    bitmasks.emplace_back(num2);
  } else {
    // remove last element and get recursive results
    uint64_t current_bit_position = bit_positions.back();
    bit_positions.pop_back();
    std::vector<uint64_t> child_bitmasks = BitPositionsToAllPossibleBitmasks(bit_positions);
    for (uint64_t child_bitmask : child_bitmasks) {
      // take cursive number and add one state for each possible value of the current bit
      uint64_t bitmask1 = child_bitmask | (1ull << current_bit_position);
      uint64_t bitmask2 = child_bitmask;
      bitmasks.emplace_back(bitmask1);
      bitmasks.emplace_back(bitmask2);
    }
  }
  return bitmasks;
}

/// turns flippable bitmask into list of bitmasks that represent possible values for potential enums
/// e.g. CreateSlidingBitwindowMasks(0b111, 2) = [0b000, 0b001, 0b010, 0b011,
///                                              (0b000), (0b010), 0b100, 0b110]
/// \param flip_bitmask bitmask of flippable bits
/// \param window_size number of bits that are considered as one value
std::vector<uint64_t> CreateSlidingBitwindowMasks(uint64_t flip_bitmask, uint64_t window_size) {
  std::vector<uint64_t> bit_positions;
  for (uint64_t bit = 0; bit < 64; bit++) {
    if ((flip_bitmask >> bit) & 1ull) {
      bit_positions.emplace_back(bit);
    }
  }
  std::vector<uint64_t> bitmasks;
  if (window_size > bit_positions.size()) {
    window_size = bit_positions.size();
  }
  for (size_t window_pos = 0; window_pos <= bit_positions.size() - window_size; window_pos++) {
    auto window_begin = bit_positions.begin() + window_pos;
    auto window_end = bit_positions.begin() + window_pos + window_size;
    std::vector<uint64_t> child_bitmasks = BitPositionsToAllPossibleBitmasks(
        std::vector<uint64_t>(window_begin, window_end));
    bitmasks.insert(bitmasks.end(), child_bitmasks.begin(), child_bitmasks.end());
  }
  // remove duplicates
  std::sort(bitmasks.begin(), bitmasks.end());
  bitmasks.erase(std::unique(bitmasks.begin(), bitmasks.end()), bitmasks.end());
  return bitmasks;
}

std::vector<uint64_t> CreateAllPossibleUnifiedBitmasks(uint64_t window_size) {
  std::vector<uint64_t> bitmasks;
  for (uint64_t windowvalue = 0; windowvalue < std::pow(2, window_size); windowvalue++) {
    uint64_t bitmask = 0;
    for (uint64_t offset = 0; offset < kMSRMaxBitSize; offset += window_size) {
      bitmask |= windowvalue << offset;
    }
    bitmasks.emplace_back(bitmask);
  }
  return bitmasks;
}

/// Creating masks to test multiple bitwindows at once
std::vector<uint64_t> CreateUnifiedBitwindowMasks(uint64_t flip_bitmask, uint64_t window_size) {
  std::vector<uint64_t> bit_positions;
  for (uint64_t bit = 0; bit < 64; bit++) {
    if ((flip_bitmask >> bit) & 1ull) {
      bit_positions.emplace_back(bit);
    }
  }

  std::vector<uint64_t> possible_unified_bitmasks = CreateAllPossibleUnifiedBitmasks(window_size);
  std::vector<uint64_t> flippable_bitmasks;
  for (uint64_t possible_bitmask : possible_unified_bitmasks) {
    uint64_t bitmask = 0;
    for (uint64_t bitposition = 0; bitposition < 64; bitposition++) {
      if ((flip_bitmask >> bitposition) & 1ull) {  // skip non-flippable bits
        bitmask |= (possible_bitmask & 1ull) << bitposition;  // take last bit possible mask
        possible_bitmask >>= 1ull;  // cut of last mask bit as we already used it
      }
    }
    flippable_bitmasks.emplace_back(bitmask);
  }
  // remove duplicates (can happen as resulting masks are smaller than original ones)
  std::sort(flippable_bitmasks.begin(), flippable_bitmasks.end());
  flippable_bitmasks.erase(std::unique(flippable_bitmasks.begin(), flippable_bitmasks.end()),
                           flippable_bitmasks.end());
  return flippable_bitmasks;
}

PMCDifferences UnionSideEffects(PMCDifferences observed1, PMCDifferences observed2) {
  PMCDifferences union_diffs;
  for (auto&[thresholds1, observed_value1] : observed1) {
    std::string effect1 = thresholds1.test_name + "---" + thresholds1.pmc_name;
    for (auto&[thresholds2, observed_value2] : observed2) {
      std::string effect2 = thresholds2.test_name + "---" + thresholds2.pmc_name;
      if (effect1 == effect2) {
        // we just keep the first value (shouldn't matter much)
        union_diffs.emplace_back(thresholds1, observed_value1);
      }
    }
  }
  return union_diffs;
}
