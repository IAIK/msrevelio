#include "phase2.h"

#include <cstdlib>
#include <string>
#include <unordered_map>
#include <fstream>
#include <utility>
#include <vector>
#include <sstream>

#include "common.h"
#include "utils.h"

void WriteOutSideEffects(const std::string& filename, const Phase2Results& results) {
  std::ofstream fd(filename);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open " + filename + "! Aborting! ");
    exit(1);
  }
  std::string headerline = "msr;flipped_bits;(" + PMCReferenceThreshold::GetCSVHeaderline()
      + ";observed_value;)*";
  fd << headerline << std::endl;
  for (const Phase2Result& result : results) {
    fd << ToHex(result.msr) << ";" << ToHex(result.flipped_bits);
    for (auto&[thresholds, observed_value] : result.observed_sideeffects) {
      fd << ";" << thresholds.ToCSV() << ";" << observed_value;
    }
    fd << std::endl;
  }
}

Phase2Results ReadInSideEffects(const std::string& filename) {
  std::ifstream fd(filename);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open " + filename + "! Aborting! Try running --phase2");
    exit(1);
  }
  std::string headerline = "msr;flipped_bits;(" + PMCReferenceThreshold::GetCSVHeaderline()
      + ";observed_value;)*";
  std::string line;
  std::getline(fd, line);
  if (line != headerline) {
    PrintErrorMessage("File " + filename + " has an incorrect format (0)");
    exit(1);
  }

  // PMCReferenceCSV;observed_value
  size_t threshold_csv_length = PMCReferenceThreshold::GetCSVLength();
  size_t variable_csv_block_length = threshold_csv_length + 1;
  Phase2Results results;
  while (std::getline(fd, line)) {
    auto line_splitted = SplitString(line, ';');
    if ((line_splitted.size() - 2) % variable_csv_block_length != 0) {
      PrintErrorMessage("File " + filename + " has an incorrect format (1)");
      exit(1);
    }
    PMCDifferences observed_sideeffects;
    try {
      uint32_t msr = std::stoull(line_splitted[0], nullptr, 16);
      uint32_t flipped_bits = std::stoull(line_splitted[1], nullptr, 16);
      if (line_splitted.size() > 2) {
        for (size_t i = 2;  // skip msr and flipped_bits entry
             i < line_splitted.size() - threshold_csv_length;
             i += variable_csv_block_length) {
          PMCReferenceThreshold reference_threshold;
          std::vector<std::string> threshold_csv(line_splitted.begin() + i,
                                                 line_splitted.begin() + i + threshold_csv_length);
          int err = reference_threshold.LoadFromCSV(threshold_csv);
          if (err) {
            PrintErrorMessage("File " + filename + " has an incorrect format (2)");
            exit(1);
          }
          double observed_value = std::stod(line_splitted[i + threshold_csv_length]);
          observed_sideeffects.emplace_back(reference_threshold, observed_value);
        }
      }
      results.emplace_back(msr, observed_sideeffects, flipped_bits);
    } catch (std::exception& e) {
      PrintErrorMessage(e.what());
      PrintErrorMessage("File " + filename + " has an incorrect format (3)");
      exit(1);
    }
  }  // while (std::getline(fd, line)) {
  return results;
}

PMCDifferences CheckForSideEffects(const Phase1Result& msr_test_result,
                                   uint64_t bits_to_flip,
                                   uint32_t core_id,
                                   bool keep_write_only_msrs,
                                   const MSRInterface& msr_interface,
                                   const std::string& instruction_folder,
                                   const PMCReferenceThresholdMap& reference_threshold_map) {
  PMCDifferences empty_vector;
  if (msr_test_result.flip_bitmask == 0 || msr_test_result.error_code != ErrorCode::kSuccess) {
    // in this case we cannot flip anything
    return empty_vector;
  }

  std::string pmc_test_results("TODO_phase2_pmc_test_results");

  // flip all flippable bits
  //int err = FlipBits(msr_test_result.msr, msr_test_result.flip_bitmask, msr_interface);
  int err = FlipBits(msr_test_result.msr,
                     bits_to_flip,
                     false,
                     keep_write_only_msrs,
                     msr_interface);
  if (err) {
    PrintWarningMessage("Skipping MSR " + ToHex(msr_test_result.msr) + ".");
    return empty_vector;
  }

  // measure PMCs
  err = RecordPMCs(core_id, instruction_folder, pmc_test_results);
  if (err) {
    PrintWarningMessage("Error recording PMC events. Skipping MSR.");
    return empty_vector;
  }

  // flip back all flippable bits
  //err = FlipBits(msr_test_result.msr, msr_test_result.flip_bitmask, msr_interface);
  err = FlipBits(msr_test_result.msr,
                 bits_to_flip,
                 true,
                 keep_write_only_msrs,
                 msr_interface);
  if (err) {
    PrintErrorMessage("Error resetting state. Aborting!");
    exit(1);
  }

  return CheckForPMCDifference(reference_threshold_map, pmc_test_results);
}

Phase2Results SearchForSideEffects(const Phase1Results& test_results,
                                   const std::string& pmc_threshold_file,
                                   bool keep_write_only_msrs,
                                   uint32_t core_id) {
  MSRInterface msr_interface(core_id);
  PMCReferenceThresholdMap pmc_reference_thresholds = ReadInPMCThresholds(pmc_threshold_file);

  Phase2Results phase_2_results;
  for (Phase1Result msr_test_result : test_results) {
    PrintInfoMessage("Checking MSR " + ToHex(msr_test_result.msr));
    // TODO: export this 'if' into some config or so
    if (msr_test_result.msr == 0x140) {
      PrintWarningMessage("Skipping MSR because it breaks nanoBench.");
      continue;
    }

    // get all possible bit windows that we should flip
    //std::vector<uint64_t> list_to_flip = CreateSlidingBitwindowMasks(
    //    msr_test_result.flip_bitmask,
    //    kBitWindowSize);
    std::vector<uint64_t> list_to_flip = CreateUnifiedBitwindowMasks(
        msr_test_result.flip_bitmask,
        kBitWindowSize);
    for (uint64_t bits_to_flip : list_to_flip) {
      if (bits_to_flip == 0) {
        // ignore cases in which we would not flip anything
        continue;
      }

      PMCDifferences observed_sideeffects_run1 =
          CheckForSideEffects(msr_test_result,
                              bits_to_flip,
                              core_id,
                              keep_write_only_msrs,
                              msr_interface,
                              kInstructionFolder,
                              pmc_reference_thresholds);

      PMCDifferences observed_sideeffects_run2;
      PMCDifferences observed_sideeffects_union12;
      if (!observed_sideeffects_run1.empty()) {
        observed_sideeffects_run2 = CheckForSideEffects(msr_test_result,
                                                        bits_to_flip,
                                                        core_id,
                                                        keep_write_only_msrs,
                                                        msr_interface,
                                                        kInstructionFolder,
                                                        pmc_reference_thresholds);
        observed_sideeffects_union12 = UnionSideEffects(observed_sideeffects_run1,
                                                        observed_sideeffects_run2);
      }
      PMCDifferences observed_sideeffects_run3;
      PMCDifferences observed_sideeffects_union123;
      if (!observed_sideeffects_union12.empty()) {
        observed_sideeffects_run3 = CheckForSideEffects(msr_test_result,
                                                        bits_to_flip,
                                                        core_id,
                                                        keep_write_only_msrs,
                                                        msr_interface,
                                                        kInstructionFolder,
                                                        pmc_reference_thresholds);
        observed_sideeffects_union123 = UnionSideEffects(observed_sideeffects_union12,
                                                         observed_sideeffects_run3);
      }

      if (!observed_sideeffects_union123.empty()) {
        PrintSuccessMessage("Found difference for MSR " + ToHex(msr_test_result.msr)
                                + " by flipping bits " + ToHex(bits_to_flip));
        phase_2_results.emplace_back(msr_test_result.msr,
                                     observed_sideeffects_union123,
                                     bits_to_flip);
        // we break as we only want to add an "interesting" msr one single time
        break;
      }
    }
  }
  return phase_2_results;
}

