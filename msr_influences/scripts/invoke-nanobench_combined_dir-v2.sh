#!/bin/bash

# directories
invoke_directory=`pwd`
script_directory=$invoke_directory/`dirname "$0"`

cfg=configs/cfg_Skylake_all.txt
#cfg=configs/cfg_Zen_all.txt

core=$1
input_dir=$2
output=$3

loop_count=10

nanobench_dir=/home/user/tools/nanoBench/

retvalue=0
# get names of counters
cd $nanobench_dir
printf "NAME;" > $invoke_directory/$output
sudo ./kernel-nanoBench.sh -cpu $core -asm_init "shr R14, 5; shl R14,5" -asm "add RAX, RAX" -config $cfg | cut -d':' -f1 | tr '\n' ';' | sed 's/.$//' >> $invoke_directory/$output
errcode=${PIPESTATUS[0]}
if [ $errcode -ne 0 ]; then
  retvalue=$errcode
fi
printf "\n" >> $invoke_directory/$output

#sudo cpupower freequency-set -c $core -f 4.0Ghz -r

for file in $invoke_directory/$input_dir/*.csv; do

  name="$(basename -- $file)"
  data=$(cat $file)

  echo "[*] invoke-nanoBench: running $name"
  printf "$name;" >> $invoke_directory/$output
  sudo ./kernel-nanoBench.sh -cpu $core -asm_init "shr R14, 5; shl R14,5" -asm "$data" -config $cfg -df -n_measurements $loop_count -unroll_count 100 | cut -d':' -f2 | tr '\n' ';' | sed 's/.$//' >> $invoke_directory/$output
  errcode=${PIPESTATUS[0]}
  if [ $errcode -ne 0 ]; then
    retvalue=$errcode
  fi
  printf "\n" >> $invoke_directory/$output
done;
exit $retvalue





