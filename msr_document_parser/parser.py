#!/usr/bin/env python3

import re
import json

import click
from poppler import load_from_file


#re_msr_description_line = re.compile('(^\s*MSR[0-9A-F]+_[0-9A-F]+)([[0-9A-F]+...[0-9A-F]+][0-9A-F]?)?\ (\[.*\])\ (\(.*\))')
re_amd_msr_description_line = re.compile('^\s*(MSR[0-9A-F]+_[0-9A-F]+)([0-9A-F]+...[0-9A-F]+)?\ (.*)')
re_amd_msr_bitfield_line = re.compile('(^\s*[0-9]+(:[0-9]+)?)')
re_amd_not_table_line = re.compile('^[a-zA-z]+.*$')


def parse_amd(pdf_document, reserved):
    msrs = {}

    full_text = ""

    for page_idx in range(pdf_document.pages):
        page = pdf_document.create_page(page_idx)
        text = page.text()

        full_text += text

    # states
    begin_msr = False
    begin_bits = False

    # info
    msr_name = None
    msr_address = None
    bits = []
    empty_line_counter = 0

    def end_of_msr():
        nonlocal begin_bits
        nonlocal msr_name
        nonlocal msr_address
        nonlocal bits

        msr_address = msr_address.replace("_", "")

        msr = {
            'name': msr_name,
            'address': "0x" + msr_address,
            'bits': bits
        }

        if msr_name and msr_address:
            if msr_address not in msrs:
                msrs[msr_address] = []
            msrs[msr_address].append(msr)

        begin_bits = False
        msr_name = None
        msr_address = None
        bits = []

    for line in full_text.split("\n"):
        # check if MSR begins
        match = re_amd_msr_description_line.findall(line)
        if match:
            if 'is an alias of' in line or 'is aliased to' in line:
                continue

            if begin_msr:
                end_of_msr()

            begin_msr = True
            msr_name = match[0][2]
            msr_address = match[0][0][3:]

            if match[0][1] != '': # multi-msr
                msr_address += match[0][1]

            continue

        # check rest of msr content
        if begin_msr is True:
            if 'Bits Description' in line:
                begin_bits = True
                continue

            if begin_bits is True:
                match = re_amd_msr_bitfield_line.findall(line) # description of bits
                if match:
                    msr_bits = match[0][0].strip()
                    if reserved:
                        if 'Reserved.' in line:
                            bits.append(msr_bits)
                    else:
                        bits.append(msr_bits)

            if len(line) == 0:
                empty_line_counter += 1

        if empty_line_counter > 10:
            empty_line_counter = 0
            end_of_msr()

    end_of_msr()

    combined = []


    for nr, msrs in msrs.items():
        mask = 0
        for msr in msrs:
            for rev in msr["bits"]:
                p = rev.split(":")
                if len(p) == 2:
                    end, begin = int(p[0]), int(p[1])

                elif len(p) == 1:
                    end = begin = int(p[0])

                t = (1 << (end+1)) - 1 
                    
                t2 = (1 << (begin)) - 1
                t = t & ~t2
                mask |= t
        names = {}
        for msr in msrs:
            if not msr["name"] in names:
                combined.append({
                            'name': msr["name"],
                            'address': msr["address"],
                            #'bits': msr["bits"],
                            'reserved_mask': hex(mask)
                })
            names[msr["name"]] = 1

    return combined

re_intel_msr_description_line = re.compile('^\s*([0-9A-F]+)H\s*[0-9]+\s*([^\s]*)\s*(.*)$')
re_intel_msr_bitfield_line = re.compile('^\s*([0-9]+:?\ ?[0-9]+)\s+(.*)$')

re_intel_msr_description_line_mline = [re.compile('^\s*([0-9A-F]+)_\s*((IA32_|MSR_)[^\s]*)\s*(.*)$'), re.compile('^\s*([0-9A-F]+)H(.*)$')]

import subprocess

def pdftotext(path):
    cp = subprocess.run(['pdftotext', '-layout', path, '-'], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True, text=True)
    return cp.stdout


def parse_intel(pdf_document, reserved):
    msrs = {}

    # states
    begin_msr = False
    begin_msr_table = False
    begin_bits = False

    # info
    msr_name = None
    msr_address = None
    bits = []

    #for page_idx in range(pdf_document.pages):
    #    page = pdf_document.create_page(page_idx)
    #    text = page.text()

    text = pdftotext(pdf_document)


    multiline = False

    for line in text.split("\n"):
        # check if MSR begins
        if 'Register Name' in line or "MSR Name" in line:
            begin_msr_table = True
            continue
        elif not begin_msr_table:
            continue

        if multiline:
            match = re_intel_msr_description_line_mline[1].findall(line)
            if not match:
                print("error")
                exit(0)
            msr_address += match[0][0]
            #print(f"multiline address {msr_address}")
            multiline = False
        else:

            match = re_intel_msr_description_line.findall(line)
        
            if not match:
                match = re_intel_msr_description_line_mline[0].findall(line)
                if match:
                    multiline = True

        
        if match:
            if begin_msr is True:
                msr = {
                    'name': msr_name,
                    'address': "0x" + msr_address,
                    'bits': bits
                }

                #if len(bits) > 0:
                if msr_address not in msrs:
                    msrs[msr_address] = []
                    
                msrs[msr_address].append(msr)

                begin_bits = False
                msr_name = None
                msr_address = None
                bits = []

            # new msr
            msr_name = match[0][1]
            msr_address = match[0][0]

            begin_msr = True
            continue

        # sometimes the manual refers to MAXPHYADDR, e.g., IA32_APIC_BASE description
        # we just replace that by the highest bit of an MSR
        line = line.replace("MAXPHYADDR", "63")
        match = re_intel_msr_bitfield_line.findall(line) # description of bits
        if match:
            if "309" in line:
                print(match)
            msr_bits = match[0][0].strip().replace(' ', '')
            if reserved:
                if msr_name == "MSR_PLATFORM_INFO" and "03: Reserved" in line:
                    print(line, msr_bits)
                if 'Reserved' in line:
                    bits.append(msr_bits)

    if msr_name is not None:
        msr = {
            'name': msr_name,
            'address': "0x" + msr_address,
            'bits': bits
        }

        #if len(bits) > 0:
        if msr_address not in msrs:
            msrs[msr_address] = []
            
        msrs[msr_address].append(msr)


    combined = []


    for nr, msrs in msrs.items():
        mask = 0
        for msr in msrs:
            for rev in msr["bits"]:
                p = rev.split(":")
                if len(p) == 2:
                    end, begin = int(p[0]), int(p[1])

                elif len(p) == 1:
                    end = begin = int(p[0])

                t = (1 << (end+1)) - 1 
                    
                t2 = (1 << (begin)) - 1
                t = t & ~t2
                mask |= t
        names = {}
        for msr in msrs:
            if not msr["name"] in names:
                combined.append({
                            'name': msr["name"],
                            'address': msr["address"],
                            #'bits': msr["bits"],
                            'reserved_mask': hex(mask)
                })
            names[msr["name"]] = 1

    return combined


@click.command()
@click.argument('path', type=click.Path(exists=True))
@click.option('--reserved/--no-reserved', default=False)
def main(path, reserved):
    pdf_document = load_from_file(path)

    if 'AMD' in pdf_document.author:
        msrs = parse_amd(pdf_document, reserved)
    elif 'Intel' in pdf_document.author:
        msrs = parse_intel(path, reserved)
    else:
        print("Could not detect PDF author")
        return -1


    for msr in msrs:
        print(json.dumps(msr))

if __name__ == "__main__":
    main()
