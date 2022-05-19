#ifndef MSR_UOPS_RUNNERV2_COMMON_H_
#define MSR_UOPS_RUNNERV2_COMMON_H_

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <fcntl.h>

#include "utils.h"

//
// global constants
//
extern const std::string kInstructionFolder;
extern const int kBitWindowSize;

//
// datastructures
//
struct msr_context {
  uint32_t address;
  std::string name;
  std::vector<int> bits_to_check;
};

int CountBitsToCheck(const std::vector<msr_context>& msrs);

struct PMCReferenceThreshold {
  std::string test_name;
  std::string pmc_name;
  double reference_value;
  double lower_threshold;
  double upper_threshold;
  PMCReferenceThreshold() = default;
  PMCReferenceThreshold(std::string test_name_value,
                        std::string pmc_name_value,
                        double ref_value,
                        double lower_threshold_value,
                        double upper_threshold_value)
      : test_name(std::move(test_name_value)),
        pmc_name(std::move(pmc_name_value)),
        reference_value(ref_value),
        lower_threshold(lower_threshold_value),
        upper_threshold(upper_threshold_value) {}

  static std::string GetCSVHeaderline() {
    return "testname;pmcname;reference_value;lower_threshold;upper_threshold";
  }
  static size_t GetCSVLength() {
    std::string headerline = GetCSVHeaderline();
    return std::count(headerline.begin(), headerline.end(), ';') + 1;
  }

  std::string ToCSV() const {
    std::stringstream ss;
    ss << test_name << ";" << pmc_name << ";"
       << std::to_string(reference_value) << ";"
       << std::to_string(lower_threshold) << ";"
       << std::to_string(upper_threshold);
    return ss.str();
  }

  int LoadFromCSV(const std::string& csv_line) {
    auto splitted_line = SplitString(csv_line, ';');
    return LoadFromCSV(splitted_line);
  }

  int LoadFromCSV(std::vector<std::string> csv_line_splitted) {
    if (csv_line_splitted.size() != 5) {
      return 1;
    }
    test_name = csv_line_splitted[0];
    pmc_name = csv_line_splitted[1];
    try {
      reference_value = std::stod(csv_line_splitted[2]);
      lower_threshold = std::stod(csv_line_splitted[3]);
      upper_threshold = std::stod(csv_line_splitted[4]);
    } catch (...) {
      return 1;
    }
    return 0;
  }
};

class MSRInterface {
 public:
  explicit MSRInterface(uint32_t core_id) {
    this->core_id_ = core_id;
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "/dev/cpu/%u/msr", core_id);
    fd_ = open(buffer, O_RDWR);
    if (fd_ < 0) {
      PrintErrorMessage("Could not open MSR dev on core " + ToHex(core_id) + ". Aborting!");
      exit(1);
    }
  }

  ~MSRInterface() {
    close(fd_);
  }

  int Read(uint32_t msr, uint64_t* value) const {
    return pread(fd_, value, sizeof(uint64_t), msr) != sizeof(uint64_t);
  }

  int Write(uint32_t msr, uint64_t value) const {
    return pwrite(fd_, &value, sizeof(uint64_t), msr) != sizeof(uint64_t);
  }

  uint32_t GetCoreID() const {
    return core_id_;
  }

 private:
  int fd_;
  uint32_t core_id_;
};

//
// type declarations
//
using PMCDifferences = std::vector<std::pair<PMCReferenceThreshold, double>>;
using PMCReferenceThresholdMap = std::unordered_map<std::string, PMCReferenceThreshold>;

//
// functions
//
void SetCPUAffinity(int cpu);
int FlipBits(uint32_t msr,
             uint64_t bitmask_to_flip,
             bool restore_old_value,
             bool accept_write_only_msrs,
             const MSRInterface& msr_interface);
int RecordPMCs(uint32_t core_id, const std::string& instruction_folder,
               const std::string& output_file);
std::string CreatePMCReferenceThresholdMapKey(const std::string& testname,
                                              const std::string& pmcname);
std::string CreatePMCReferenceThresholdMapKey(const PMCReferenceThreshold& threshold);
PMCReferenceThresholdMap CreatePMCReferenceThresholds(
    uint32_t core_id,
    const std::string& instruction_folder);
void WriteOutPMCThresholds(const std::string& filename,
                           const PMCReferenceThresholdMap& pmc_reference_thresholds);
PMCReferenceThresholdMap ReadInPMCThresholds(const std::string& filename);
PMCDifferences CheckForPMCDifference(PMCReferenceThresholdMap reference_threshold_map,
                                     const std::string& test_results);
void CalibrateReferenceThresholds(const std::string& pmc_threshold_file,
                                  uint32_t core_id,
                                  uint32_t maximum_calibrate_tries);


/// turns flippable bitmask into list of bitmasks that represent possible values for potential enums
/// e.g. BitmaskToPossibleBitWindows(0b111, 2) = [0b000, 0b001, 0b010, 0b011,
///                                              (0b000), (0b010), 0b100, 0b110]
/// \param flip_bitmask bitmask of flippable bits
/// \param window_size number of bits that are considered as one value
std::vector<uint64_t> CreateSlidingBitwindowMasks(uint64_t flip_bitmask,
                                                  uint64_t window_size);

/// Creating masks to test multiple bitwindows at once
std::vector<uint64_t> CreateUnifiedBitwindowMasks(uint64_t flip_bitmask, uint64_t window_size);

// Calculate the union of two runs of side effects
PMCDifferences UnionSideEffects(PMCDifferences observed1, PMCDifferences observed2);
#endif // MSR_UOPS_RUNNERV2_COMMON_H_
