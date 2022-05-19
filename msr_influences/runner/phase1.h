#ifndef MSR_UOPS_RUNNERV2_PHASE1_H_
#define MSR_UOPS_RUNNERV2_PHASE1_H_

#include <map>
#include <string>

#include "common.h"
#include "utils.h"

enum class ErrorCode {
  kSuccess,
  kInitialReadError,
  kReadAfterFailedWriteError,
  kValueChangeAfterFailedWriteError,
  kReadAfterSuccessfulWriteError,
  kRestoreError
};

struct Phase1Result {
  uint32_t msr;
  ErrorCode error_code;
  uint64_t flip_bitmask;

  Phase1Result(uint32_t msr_value, ErrorCode error_code_value, uint64_t flip_bitmask_value)
      : msr(msr_value), error_code(error_code_value), flip_bitmask(flip_bitmask_value) {}
};
using Phase1Results = std::vector<Phase1Result>;

void PrintStatisticsForWritableBits(const Phase1Results& results);

std::string ErrorCodeToString(ErrorCode error_code);
ErrorCode StringToErrorCode(const std::string& error_code);

Phase1Results SearchFlippableBits(const std::vector<msr_context>& msrs,
                                  uint32_t core_id,
                                  bool keep_write_only_msrs);

void WriteOutFlippableBits(const std::string& filename, const Phase1Results& flip_results);
Phase1Results ReadInFlippableBits(const std::string& filename);

#endif // MSR_UOPS_RUNNERV2_PHASE1_H_
