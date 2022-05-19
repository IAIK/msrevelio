import sys
import struct
import time
import json

REPEAT = 5

def read_msr(msr_index, cpu_index=None):
    if cpu_index is not None:
        cpus = ["/dev/cpu/%d/msr" % cpu_index]
    else:
        cpus = sorted(list(glob.glob("/dev/cpu/*/msr")))

        if not cpus:
            print("No CPUs found, did you run 'modprobe msr'?")
            return -1
    
    val = []
    for cpu_msr in cpus:
        try:
            cpu_msr_dev = open(cpu_msr, "rb")
        except IOError:
            print("Unable to open %s for reading. You need to be root." % cpu_msr)
            return [-1] * len(cpus)
        try:
            cpu_msr_dev.seek(msr_index, 0)
            msr = cpu_msr_dev.read(8)
            msr_value = struct.unpack("<Q", msr)[0]
        except:
            return [-1] * len(cpus)
        cpu_msr_dev.close()
        val.append(msr_value)
    return val


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: %s <list> <comment>" % sys.argv[0])
        sys.exit(1)

    msrs = open(sys.argv[1]).read().strip().split("\n")
    print("[+] Scanning %d MSRs" % len(msrs))
    msr_val = {}
    for r in range(REPEAT):
        for msr in msrs:
            idx = int(msr, 16)
            msr_val[idx] = msr_val.get(idx, []) + [ read_msr(int(msr, 16), 0)[0] ]
        time.sleep(1)
    print("[+] Analyzing...")
    static = 0
    dynamic = 0
    for msr in msr_val:
        msr_val[msr] = list(set(msr_val[msr]))
        if len(msr_val[msr]) > 1:
            dynamic +=1
            msr_val[msr] = [-2] * REPEAT
        else:
            static += 1
    print("[+] %d static, %d dynamic" % (static, dynamic))
    
    try:
        db = json.load(open("results.json"))
    except:
        db = {"recordings": [], "name": []}
        
    db["recordings"].append(msr_val)
    db["name"].append(sys.argv[2])
    
    json.dump(db, open("results.json", "w"))
    
    
