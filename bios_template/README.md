## TL;DR
* Use the msrs_detect and msrs_ls to get a list of available MSRs, save as msrs.txt
* Load msr kernel module (sudo modprobe msr)
* Run `sudo python3 static_log.py msrs.log <tag>` to record all MSR values, store as "<tag>"
* Enable/disable a feature in the BIOS
* Run `sudo python3 static_log.py msrs.log <tag2>` to record all MSR values, store as "<tag2>"
* Repeat with as many features as you want (remember to always use a new tag!)
* Run `python3 diff.py results.json` to see MSRs that have different values for different tags

## Example
* Enable all features in BIOS
* Run `sudo python3 static_log.py msrs.txt all`
* Disable turbo boost
* Run `sudo python3 static_log.py msrs.txt no-turbo`
* Run `python3 diff.py results.json`
```
     1a0: 
          - 850089: all
          - 4000850089: no-turbo
```
