#!/bin/bash

USAGE="Usage: ${0} sample_time msr_file"

timestamp=$(date +"%Y%m%d-%H%M%S")

sampler_path=./msr_scanner/tools/build/bin/msrs_sample

set -e

[[ -z "${1}" ]] && { echo "Missing argument: sample time!" ; echo $USAGE ; exit 1 ; }
[[ -z "${2}" ]] && { echo "Missing argument: path to msr file!" ; echo $USAGE ; exit 1 ; }

sed "/^#/d" ${2} | sed "s/^/0x/" | sudo xargs ${sampler_path} ${1} > sample_all_${1}_${timestamp}
