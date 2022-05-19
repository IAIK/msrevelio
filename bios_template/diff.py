import json
import sys
import re

def extract_feature(x):
    return re.sub("[0-9]+", "", x)

if __name__ == "__main__":
    db = json.load(open(sys.argv[1]))
    blacklist = []
    try:
        blacklist = open(sys.argv[2]).read().strip().split("\n")
    except:
        pass
    recordings = len(db["name"])
    features = list(set([ extract_feature(x) for x in db["name"] ]))
    print("[+] Found %d recordings" % recordings)
    msrs = []
    for record in db["recordings"]:
        msrs += record.keys()
    msrs = list(set(msrs))
    print("[+] Found %d MSRs" % len(msrs))
    for msr in msrs:
        val = []
        for i in range(recordings):
            val += db["recordings"][i][msr]
        if len(set(val)) > 1:
            hist = {}
            for i in range(recordings):
                v = db["recordings"][i][msr][0]
                hist[v] = hist.get(v, []) + [ extract_feature(db["name"][i]) ]
            # if a feature has more than one value -> delete it, "noise"
            for f in features:
                count = 0
                for v in hist:
                    if f in hist[v]:
                        count += 1
                if count > 1:
                    for v in hist:
                        if f in hist[v]:
                            hist[v].remove(f)

            hist_filtered = {}
            for v in hist:
                fs = list(set(hist[v]))
                if len(fs) >= 1:
                    hist_filtered[v] = fs

            if len(hist_filtered) > 1 and ("%x" % int(msr)) not in blacklist:
                print("%8x: " % int(msr))
                for v in hist_filtered:
                    print("          - %x: %s" % (v, ", ".join(hist_filtered[v])))
