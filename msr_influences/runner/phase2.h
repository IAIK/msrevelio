#ifndef MSR_UOPS_RUNNERV2_PHASE2_H_
#define MSR_UOPS_RUNNERV2_PHASE2_H_

#include <algorithm>
#include <string>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "phase1.h"
#include "common.h"

struct Phase2Result {
  uint32_t msr;
  PMCDifferences observed_sideeffects;
  uint64_t flipped_bits = 0;
  Phase2Result(uint32_t msr_value,
               PMCDifferences observed_sideeffects_value)
      : msr(msr_value),
        observed_sideeffects(std::move(observed_sideeffects_value)) {}
  Phase2Result(uint32_t msr_value,
               PMCDifferences observed_sideeffects_value,
               uint64_t flipped_bits_value)
      : msr(msr_value),
        observed_sideeffects(std::move(observed_sideeffects_value)),
        flipped_bits(flipped_bits_value) {}
};
using Phase2Results = std::vector<Phase2Result>;

Phase2Results SearchForSideEffects(const Phase1Results& test_results,
                                   const std::string& pmc_threshold_file,
                                   bool keep_write_only_msrs,
                                   uint32_t core_id);
void CalibrateReferenceThresholds(const std::string& pmc_threshold_file,
                                  uint32_t core_id,
                                  uint32_t maximum_calibrate_tries);

Phase2Results ReadInSideEffects(const std::string& filename);
void WriteOutSideEffects(const std::string& filename, const Phase2Results& results);

#endif // MSR_UOPS_RUNNERV2_PHASE2_H_
