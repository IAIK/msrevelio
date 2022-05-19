

# Finding and Exploiting CPU Features using MSRTemplating

This repository contains the source code for the MSRevilio framework from the Symposium on Security and Privacy 2022 paper [Finding and Exploiting CPU Features using MSRTemplating](https://andreaskogler.com/papers/msrtemplating.pdf).

This document gives a quick illustration of how to build and run the MSR scanner and additional analysis tools.

## Building the Tools

### Preparations

First, check out the submodule:

```
git submodule update --recursive --init
```

### Build and load the kernel module

To build and load the kernel module, run the following commands:

```
cd msr_scanner/module
make
sudo insmod msrs.ko
```

### Build tools

To build the necessary tools, run the following commands:

```
cd msr_scanner/tools
mkdir -p build
CC=gcc-10 CXX=g++-10 cmake ..
make
```

Note: This requires a c++ compiler with c++20 support. We suggest gcc-10 and g++-10.

## Run MSR detection

### Preperation

To run the MSR scan, you need to allow to run kernel tasks for a long period:

    echo 0 | sudo tee /proc/sys/kernel/hung_task_timeout_secs

### Run Scanner

To run the actual MSR scan that communicates with the kernel module, run the following command:

```
./msr_scanner/tools/build/bin/msrs_detect 4
```

where `4` is the number of threads that will be started.

### Extract MSR list

When the scan completes, you can extract the detected MSRs with the following command:

```
./msr_scanner/tools/build/bin/msrs_ls > msrs.log
```

## Analyze MSR list

### Sample MSRs

To sample the MSRs over a period of time, you can run the following command:

```
./scripts/sampling/sample_all.sh <duration in seconds> <msrs list file>
```

An example:

```
./scripts/sampling/sample_all.sh 1 msrs.log
```

This will create a `sample_all_1_${date}` file.

### Analyse samples

To automatically scan the resulted log file for MSRs that change over time, run the following command:

```
python ./scripts/sampling/msrs_analyze.py sample_all_${duration}_${date} -a changes
```

## Detect influences of MSR bits on instructions

Chekout the README.md in [msr_influences](./msr_influences)

## BIOS Templating

Chekout the README.md in [bios_template](./bios_template)

