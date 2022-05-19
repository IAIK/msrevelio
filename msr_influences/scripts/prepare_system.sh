#! /bin/sh

# load kernel modules
sudo modprobe msr
sudo insmod ~/tools/nanoBench/kernel/nb.ko


# reduce noise on system
#sudo ~/tools/nanoBench/disable-HT.sh
echo "performance" | sudo tee /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor

# disable crosstalk mitigations
sudo wrmsr -a 0x123 0x1

# stop GUI
sudo init 3
