#include <unistd.h>
#include <getopt.h>

#include <cstdlib>
#include <iostream>
#include <numeric>
#include <vector>
#include <fstream>

#include "utils.h"
#include "phase1.h"
#include "phase2.h"
#include "phase3.h"

constexpr int kCoreForMeasurements = 3;
//const std::string kUnknownMSRsFile("../../test_msrs_formatted");  // just for debugging
const std::string kUnknownMSRsFile("../unknown_msrs_formatted.csv");
//const std::string kUnknownMSRsFile("../../wo_msrs_formatted");
//const std::string kUnknownMSRsFile("../../reserved_msr_bits.csv");
const std::string kFlippableMSRsFile("./flippable_msrs.csv");
const std::string kPMCThresholdsFile("./pmc_thresholds.csv");
const std::string kPMCDifferencesFile("./observed_sideeffects.csv");
const std::string kTracedBitsFile("./traced_bits.csv");

std::vector<int> ParseBitsToCheck(const std::string& bits_to_check_encoded) {
  // example input: 9-63,24-63,9-22,6-7,1-4
  std::vector<int> bits;
  auto ranges = SplitString(bits_to_check_encoded, ',');
  for (const std::string& range : ranges) {
    std::vector splitted_range = SplitString(range, '-');
    if (splitted_range.size() != 2) {
      PrintErrorMessage("Ill-formatted bit range ('" + range + "'). Aborting!");
      exit(1);
    }
    try {
      uint8_t begin = std::stoi(splitted_range[0], nullptr, 0);
      uint8_t end = std::stoi(splitted_range[1], nullptr, 0);
      for (int bit = begin; bit <= end; bit++) {
        bits.push_back(bit);
      }
    } catch (const std::invalid_argument& e) {
      PrintErrorMessage("Ill-formatted bit range ('" + range + "'). Aborting!");
      exit(1);
    }
  }
  // sort bits to make further computations easier to follow
  std::sort(bits.begin(), bits.end());
  return bits;
}

std::vector<msr_context> ParseMSRFile(const std::string& filename) {
  std::ifstream fd(filename);
  if (!fd.is_open()) {
    std::cerr << "Failed reading file " << filename << std::endl;
    std::exit(1);
  }

  std::vector<msr_context> parsed_msrs;
  std::string line;
  std::stringstream ss;
  std::string headerline = "msr;name;bits-to-check";
  std::getline(fd, line);
  if (line != headerline) {
    PrintErrorMessage("File " + filename + " has an incorrect format");
    exit(1);
  }

  while (std::getline(fd, line)) {
    auto splitted_line = SplitString(line, ';');
    msr_context msr;
    msr.address = (std::stoul(splitted_line[0], nullptr, 16));
    msr.name = splitted_line[1];
    msr.bits_to_check = ParseBitsToCheck(splitted_line[2]);
    parsed_msrs.push_back(msr);
  }
  std::cout << "[+] Read " << parsed_msrs.size() << " MSRs from MSR-File" << std::endl;
  return parsed_msrs;
}

struct CommandLineArguments {
  bool phase1 = false;
  bool phase2 = false;
  bool phase3 = false;
  bool calibrate = false;
  bool msr = false;
  bool keep_write_only_msrs = false;
  uint32_t msr_value;
};

void PrintHelp(char** argv) {
  std::cout << "USAGE: " << argv[0] << " [OPTION]" << std::endl
            << "Possible Arguments:" << std::endl
            << "--phase1 \t Search for bits that are flippable" << std::endl
            << "--phase2 \t Search for side effects of flipped bits" << std::endl
            << "--phase3 \t Trace side effects to single bit flips" << std::endl
            << "--calibrate \t Calibrate thresholds for phase2" << std::endl
            << "--keepwo \t Keep write-only MSRs (during phase1)" << std::endl
            << "--msr <MSR-NO>\t Test only specific MSR" << std::endl
            << "--help/-h \t Print usage" << std::endl;
}

CommandLineArguments ParseArguments(int argc, char** argv) {
  CommandLineArguments command_line_arguments;
  const struct option long_options[] = {
      {"phase1", no_argument, nullptr, '1'},
      {"phase2", no_argument, nullptr, '2'},
      {"phase3", no_argument, nullptr, '3'},
      {"msr", required_argument, nullptr, 'm'},
      {"help", no_argument, nullptr, 'h'},
      {"calibrate", no_argument, nullptr, 'c'},
      {"keepwo", no_argument, nullptr, 'k'},
      {nullptr, 0, nullptr, 0}
  };

  int option_index;
  int c;
  while ((c = getopt_long(argc, argv, "h", long_options, &option_index)) != -1) {
    switch (c) {
      case '0':
        break;
      case '1':
        command_line_arguments.phase1 = true;
        break;
      case '2':
        command_line_arguments.phase2 = true;
        break;
      case '3':
        command_line_arguments.phase3 = true;
        break;
      case 'c':
        command_line_arguments.calibrate = true;
        break;
      case 'k':
        command_line_arguments.keep_write_only_msrs = true;
        break;
      case 'm':
        command_line_arguments.msr = true;
        try {
          command_line_arguments.msr_value = std::stoll(optarg, nullptr, 0);
        } catch (const std::invalid_argument& e) {
          std::cout << "[-] Expected number as argument for --msr" << std::endl;
          exit(0);
        } catch (const std::out_of_range& e) {
          std::cout << "[-] Illegal msr value for --msr" << std::endl;
          exit(0);
        }
        break;
      case 'h':
      case '?':
      case ':':
        PrintHelp(argv);
        exit(0);
      default:
        std::cerr << "[-] Argument parsing failed. Aborting!" << std::endl;
        exit(1);
    }
  }
  return command_line_arguments;
}

int main(int argc, char* argv[]) {

  if (argc == 1) {
    PrintHelp(argv);
    exit(0);
  }
  CommandLineArguments command_line_arguments = ParseArguments(argc, argv);

  //
  // PHASE 1
  //
  if (command_line_arguments.phase1) {
    std::vector<msr_context> msrs;
    if (command_line_arguments.msr) {
      PrintInfoMessage("Phase1: Testing only MSR " +
          std::to_string(command_line_arguments.msr_value));
      msr_context msr;
      msr.address = command_line_arguments.msr_value;
      msr.name = "";
      // test all bits (0-63)
      std::vector<int> bits_to_check(64);
      std::iota(bits_to_check.begin(), bits_to_check.end(), 0);
      msr.bits_to_check = bits_to_check;
      msrs.push_back(msr);
    } else {
      msrs = ParseMSRFile(kUnknownMSRsFile);
    }

    PrintInfoMessage("Searching for flippable bits");
    Phase1Results phase1_results = SearchFlippableBits(msrs,
                                                       kCoreForMeasurements,
                                                       command_line_arguments.keep_write_only_msrs);
    for (auto& result : phase1_results) {
      std::cout << ToHex(result.msr)
                << " - " << ErrorCodeToString(result.error_code);
      if (result.error_code == ErrorCode::kSuccess) {
        std::cout << " - " << result.flip_bitmask;
      }
      std::cout << std::endl;
    }
    WriteOutFlippableBits(kFlippableMSRsFile, phase1_results);
    PrintStatisticsForWritableBits(phase1_results);
    PrintInfoMessage("Result stored in " + kFlippableMSRsFile);
  }

  //
  // CALIBRATE
  //
  if (command_line_arguments.calibrate) {
    PrintInfoMessage("Calibrating thresholds for PMC differences...");
    CalibrateReferenceThresholds(kPMCThresholdsFile, kCoreForMeasurements, 250);
  }

  //
  // PHASE 2
  //
  if (command_line_arguments.phase2) {
    Phase1Results phase1_results = ReadInFlippableBits(kFlippableMSRsFile);
    PrintInfoMessage("Read " + std::to_string(phase1_results.size())
                         + " MSR flip bitmasks");

    if (command_line_arguments.msr) {
      PrintInfoMessage("Phase2: Testing only MSR " +
          ToHex(command_line_arguments.msr_value));
      Phase1Results tmp_results;
      bool found = false;
      for (auto& result : phase1_results) {
        if (result.msr == command_line_arguments.msr_value) {
          tmp_results.push_back(result);
          found = true;
        }
      }
      if (!found) {
        PrintErrorMessage("[-] Could not found flip information for specified MSR. "
                          "You may want to add '--phase1'.");
        exit(1);
      }
      phase1_results = tmp_results;
    }

    PrintInfoMessage("Checking for side effects...");
    Phase2Results p2results = SearchForSideEffects(
        phase1_results,
        kPMCThresholdsFile,
        command_line_arguments.keep_write_only_msrs,
        kCoreForMeasurements);
    WriteOutSideEffects(kPMCDifferencesFile, p2results);
    PrintInfoMessage("Result stored in " + kPMCDifferencesFile);
  }

  //
  // PHASE 3
  //
  if (command_line_arguments.phase3) {
    Phase1Results phase1_results = ReadInFlippableBits(kFlippableMSRsFile);
    Phase2Results phase2_results = ReadInSideEffects(kPMCDifferencesFile);
    PrintInfoMessage("Read " + std::to_string(phase1_results.size())
                         + " MSR flip bitmasks");
    PrintInfoMessage("Read " + std::to_string(phase2_results.size())
                         + " MSRs with side effects");

    if (command_line_arguments.msr) {
      PrintInfoMessage("Phase3: Testing only MSR " +
          ToHex(command_line_arguments.msr_value));
      Phase1Results p1_tmp_results;
      Phase2Results p2_tmp_results;
      bool p1_found = false;
      bool p2_found = false;
      for (auto& result : phase1_results) {
        if (result.msr == command_line_arguments.msr_value) {
          p1_tmp_results.push_back(result);
          p1_found = true;
        }
      }
      for (auto& result : phase2_results) {
        if (result.msr == command_line_arguments.msr_value) {
          p2_tmp_results.push_back(result);
          p2_found = true;
        }
      }
      if (!p1_found) {
        PrintErrorMessage("[-] Could not found phase1 information for specified MSR. "
                          "You may want to add '--phase1'.");
        exit(1);
      }
      if (!p2_found) {
        PrintErrorMessage("[-] Could not found phase2 information for specified MSR. "
                          "You may want to add '--phase2'.");
        exit(1);
      }
      phase1_results = p1_tmp_results;
      phase2_results = p2_tmp_results;
    }

    PrintInfoMessage("Tracing side effects to flipped bits...");
    Phase3Results phase3_results = TraceSideEffectsToBits(phase1_results,
                                                          phase2_results,
                                                          kPMCThresholdsFile,
                                                          command_line_arguments.keep_write_only_msrs,
                                                          kCoreForMeasurements);
    WriteOutTracedBits(kTracedBitsFile, phase3_results);
    PrintInfoMessage("Result stored in " + kTracedBitsFile);
  }

  return 0;
}
