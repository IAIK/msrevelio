#ifndef MSR_UOPS_RUNNERV2_PHASE3_H_
#define MSR_UOPS_RUNNERV2_PHASE3_H_

#include "phase1.h"
#include "phase2.h"

struct Phase3Result {
  uint32_t msr;
  uint64_t flipped_bits;
  PMCDifferences observed_sideeffects;
  Phase3Result(uint32_t msr_value,
               uint64_t flipped_bits_val,
               PMCDifferences observed_sideeffects_value)
      : msr(msr_value),
        flipped_bits(flipped_bits_val),
        observed_sideeffects(std::move(observed_sideeffects_value)) {}
};
using Phase3Results = std::vector<Phase3Result>;

Phase3Results TraceSideEffectsToBits(const Phase1Results& phase1_results,
                                     const Phase2Results& phase2_results,
                                     const std::string& pmc_threshold_file,
                                     bool keep_write_only_msrs,
                                     uint32_t core_id);
void WriteOutTracedBits(const std::string& filename, const Phase3Results& results);

#endif // MSR_UOPS_RUNNERV2_PHASE3_H_
