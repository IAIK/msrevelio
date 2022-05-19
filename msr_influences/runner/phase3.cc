#include "phase3.h"

#include <fstream>
#include <unordered_map>

#include "phase1.h"
#include "phase2.h"

void WriteOutTracedBits(const std::string& filename, const Phase3Results& results) {
  std::ofstream fd(filename);
  if (!fd.is_open()) {
    PrintErrorMessage("Could not open " + filename + "! Aborting! ");
    exit(1);
  }

  std::string headerline = "msr;flipped_bits;" + PMCReferenceThreshold::GetCSVHeaderline()
      + ";observed_value;";
  fd << headerline << std::endl;
  for (const Phase3Result& result : results) {
    for (auto&[thresholds, observed_value] : result.observed_sideeffects) {
      fd << ToHex(result.msr) << ";" << ToHex(result.flipped_bits);
      fd << ";" << thresholds.ToCSV() << ";" << observed_value;
      fd << std::endl;
    }
  }
}

PMCDifferences CheckBitSideeffects(uint32_t msr,
                                   uint64_t bits_to_flip,
                                   bool keep_write_only_msrs,
                                   const std::string& instruction_folder,
                                   const PMCReferenceThresholdMap& reference_threshold_map,
                                   const MSRInterface& msr_interface,
                                   uint32_t core_id) {
  PMCDifferences empty_vector;

  // flip target bits
  int err = FlipBits(msr, bits_to_flip, false, keep_write_only_msrs, msr_interface);
  if (err) {
    PrintWarningMessage("Could not flip flippable bits. Skipping bits.");
    return empty_vector;
  }

  // measure PMCs
  std::string pmc_test_results("TODO_phase3_pmc_test_results");
  err = RecordPMCs(core_id, instruction_folder, pmc_test_results);
  if (err) {
    PrintWarningMessage("Error recording PMC events. Skipping bit.");
    return empty_vector;
  }

  // flip back all flippable bits
  err = FlipBits(msr, bits_to_flip, true, keep_write_only_msrs, msr_interface);
  if (err) {
    PrintErrorMessage("Error resetting state. Aborting!");
    exit(1);
  }
  return CheckForPMCDifference(reference_threshold_map, pmc_test_results);
}

Phase3Results TraceSideEffectsToBits(const Phase1Results& phase1_results,
                                     const Phase2Results& phase2_results,
                                     const std::string& pmc_threshold_file,
                                     bool keep_write_only_msrs,
                                     uint32_t core_id) {
  MSRInterface msr_interface(core_id);
  PMCReferenceThresholdMap pmc_reference_thresholds = ReadInPMCThresholds(pmc_threshold_file);
  // create bitmask mapping
  std::unordered_map<uint32_t, uint64_t> msr_to_flippable_bits;
  for (Phase1Result res : phase1_results) {
    msr_to_flippable_bits[res.msr] = res.flip_bitmask;
  }

  Phase3Results phase3_results;
  for (const Phase2Result& phase2_result : phase2_results) {
    uint32_t msr = phase2_result.msr;
    PrintInfoMessage("Checking MSR " + ToHex(msr));
    std::vector<uint64_t> bitwindows = CreateSlidingBitwindowMasks(msr_to_flippable_bits[msr],
                                                                   kBitWindowSize);
    for (uint64_t bits_to_flip : bitwindows) {
      if (bits_to_flip == 0) {
        // ignore cases in which we would not flip anything
        continue;
      }
      PrintInfoMessage("Checking MSR " + ToHex(msr) + " - bits " + ToHex(bits_to_flip));
      PMCDifferences observed_sideeffects_run1 = CheckBitSideeffects(msr,
                                                                     bits_to_flip,
                                                                     keep_write_only_msrs,
                                                                     kInstructionFolder,
                                                                     pmc_reference_thresholds,
                                                                     msr_interface,
                                                                     core_id);

      PMCDifferences observed_sideeffects_run2;
      PMCDifferences observed_sideeffects_union12;
      if (!observed_sideeffects_run1.empty()) {

        observed_sideeffects_run2 = CheckBitSideeffects(msr,
                                                        bits_to_flip,
                                                        keep_write_only_msrs,
                                                        kInstructionFolder,
                                                        pmc_reference_thresholds,
                                                        msr_interface,
                                                        core_id);
        observed_sideeffects_union12 = UnionSideEffects(observed_sideeffects_run1,
                                                        observed_sideeffects_run2);
      }
      PMCDifferences observed_sideeffects_run3;
      PMCDifferences observed_sideeffects_union123;
      if (!observed_sideeffects_union12.empty()) {
        observed_sideeffects_run3 = CheckBitSideeffects(msr,
                                                        bits_to_flip,
                                                        keep_write_only_msrs,
                                                        kInstructionFolder,
                                                        pmc_reference_thresholds,
                                                        msr_interface,
                                                        core_id);
        observed_sideeffects_union123 = UnionSideEffects(observed_sideeffects_union12,
                                                         observed_sideeffects_run3);
      }

      if (!observed_sideeffects_union123.empty()) {
        PrintSuccessMessage("Found difference for MSR " + ToHex(msr)
                                + " - bits " + ToHex(bits_to_flip));
        phase3_results.emplace_back(msr, bits_to_flip, observed_sideeffects_union123);
      }
    }
  }
  return phase3_results;
}