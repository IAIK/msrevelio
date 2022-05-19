## Z3 Solver for AESNI perfect Prime & Probe traces

To solve for the key of the provided trace file use:
```
python3 solve.py pt_0b65ba59651ff159c67f2185ef34c139.data 0b65ba59651ff159c67f2185ef34c139
```

To solve with only fewer known plaintext bytes:

```
python3 solve.py pt_0b65ba59651ff159c67f2185ef34c139.data 0b65ba59651ff159c67f2185
```