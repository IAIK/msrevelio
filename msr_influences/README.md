# Checking for changes in instruction behavior

## Setup
- Compile and load [nanoBench](https://github.com/andreas-abel/nanobench) kernel module
- Load msrtools kernel module (`sudo modprobe msr`)
- Change line 16 in `scripts/invoke-nanobench_combined_dir-v2.sh` to nanoBench installation folder
- Works best on a system with fewer noise factors (see `scripts/prepare_system.sh`)

## Build Instructions
```
cd runner
mkdir build
cd build
cmake ..
make
```

## Run
Execute the following in the build folder:
```
sudo ./msr-uops-runner --calibrate --phase1 --phase2 --phase3
```
Afterward, the results should be stored in `flippable_msrs.csv`, `observed_sideeffects.csv`, and `traced_bits.csv` for Phase 1, Phase 2, and Phase 3, respectively.
