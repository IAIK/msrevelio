# Document Parser

## Preparations

Install the required packages:

```
python3 -m pip install -r requirements.txt
```

## Extract MSRs

Execute:

```
python3 parser.py manual.pdf
```

## Tested manuals:

- **Open-Source Register Reference For AMD Family 17h Processors Models 00h-2Fh**
  - sha3=(ac6c836313a5bcecdc259aa92393e5c795c872a82f0c3aaf209c8548)

- **BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 15h Models 00h-0Fh Processors** 
  - sha3=(3ceb5c7887b03277188e1451360b27bf0d4d9483e50d0b5be95dd0bd)

- **Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 4: Model-Specific Registers**
  - sha3=(91ff0ef22d25e93ac344767c83b875cce7e258e296a499932a0ff4ce)