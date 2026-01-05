#!/usr/bin/env python3
"""
Strip unused presets from a SoundFont (.sf2) file.

Scans MIDI files to find which programs are used, then creates a minimal
soundfont containing only those presets and their required samples.

Usage:
    ./tools/strip_soundfont.py [options]
    
Options:
    --input, -i     Input soundfont (default: assets/sounds/soundfont.sf2)
    --output, -o    Output soundfont (default: assets/sounds/soundfont-stripped.sf2)
    --midi-dir, -m  Directory to scan for MIDI files (default: assets/music)
    --extra, -e     Extra program to include (can be repeated, format: "bank:program")
    --verbose, -v   Verbose output
    --dry-run       Show what would be extracted without writing

The script automatically includes the Standard drum kit (bank 128, program 0)
if any MIDI file uses channel 9 (the GM drum channel).
"""

import argparse
import os
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO


# =============================================================================
# MIDI Scanning
# =============================================================================

def scan_midi_for_programs(filepath: Path) -> tuple[set[tuple[int, int]], set[int]]:
    """
    Extract program change events and note channels from a MIDI file.
    Returns (programs, note_channels) where programs is set of (channel, program).
    """
    programs = set()  # (channel, program)
    note_channels = set()
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    i = 0
    while i < len(data) - 1:
        byte = data[i]
        
        # Program change: 0xCn pp
        if 0xC0 <= byte <= 0xCF:
            channel = byte & 0x0F
            if i + 1 < len(data):
                program = data[i + 1]
                if program < 128:  # Valid program number
                    programs.add((channel, program))
            i += 2
        # Note on: 0x9n kk vv
        elif 0x90 <= byte <= 0x9F:
            channel = byte & 0x0F
            note_channels.add(channel)
            i += 3
        else:
            i += 1
    
    return programs, note_channels


def scan_midi_directory(midi_dir: Path, verbose: bool = False) -> set[tuple[int, int]]:
    """
    Scan all MIDI files in directory and return needed (bank, program) pairs.
    """
    needed = set()
    uses_drums = False
    
    for midi_file in midi_dir.rglob("*.mid"):
        programs, channels = scan_midi_for_programs(midi_file)
        
        if verbose:
            print(f"  {midi_file.relative_to(midi_dir)}: programs={programs}, channels={channels}")
        
        # Convert channel programs to bank 0 programs
        for channel, program in programs:
            if channel != 9:  # Non-drum channels use bank 0
                needed.add((0, program))
        
        # Check if drums are used (channel 9)
        if 9 in channels:
            uses_drums = True
    
    # Add standard drum kit if drums are used
    if uses_drums:
        needed.add((128, 0))  # Standard drum kit
        if verbose:
            print("  Adding Standard drum kit (bank 128, program 0)")
    
    return needed


# =============================================================================
# SF2 Format Structures
# =============================================================================

@dataclass
class SF2Sample:
    """SF2 sample header (shdr)"""
    name: str
    start: int
    end: int
    loop_start: int
    loop_end: int
    sample_rate: int
    original_pitch: int
    pitch_correction: int
    sample_link: int
    sample_type: int
    
    # Runtime: index in new file
    new_index: int = -1
    new_start: int = 0
    new_end: int = 0
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'SF2Sample':
        name = data[0:20].rstrip(b'\x00').decode('latin-1')
        values = struct.unpack('<IIIIIBBHH', data[20:46])
        return cls(name, *values)
    
    def to_bytes(self) -> bytes:
        name_bytes = self.name.encode('latin-1')[:20].ljust(20, b'\x00')
        return name_bytes + struct.pack(
            '<IIIIIBBHH',
            self.new_start, self.new_end,
            self.new_start + (self.loop_start - self.start),
            self.new_start + (self.loop_end - self.start),
            self.sample_rate, self.original_pitch, self.pitch_correction,
            self.sample_link, self.sample_type
        )


@dataclass
class SF2Generator:
    """SF2 generator (pgen/igen)"""
    oper: int
    amount: int
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'SF2Generator':
        return cls(*struct.unpack('<HH', data))
    
    def to_bytes(self) -> bytes:
        return struct.pack('<HH', self.oper, self.amount)


@dataclass
class SF2Modulator:
    """SF2 modulator (pmod/imod)"""
    src: int
    dest: int
    amount: int
    amt_src: int
    transform: int
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'SF2Modulator':
        return cls(*struct.unpack('<HHhHH', data))
    
    def to_bytes(self) -> bytes:
        return struct.pack('<HHhHH', self.src, self.dest, self.amount, self.amt_src, self.transform)


@dataclass
class SF2Bag:
    """SF2 bag (pbag/ibag)"""
    gen_idx: int
    mod_idx: int
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'SF2Bag':
        return cls(*struct.unpack('<HH', data))
    
    def to_bytes(self) -> bytes:
        return struct.pack('<HH', self.gen_idx, self.mod_idx)


@dataclass 
class SF2Inst:
    """SF2 instrument header (inst)"""
    name: str
    bag_idx: int
    
    # Runtime
    new_index: int = -1
    new_bag_idx: int = 0
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'SF2Inst':
        name = data[0:20].rstrip(b'\x00').decode('latin-1')
        bag_idx = struct.unpack('<H', data[20:22])[0]
        return cls(name, bag_idx)
    
    def to_bytes(self) -> bytes:
        name_bytes = self.name.encode('latin-1')[:20].ljust(20, b'\x00')
        return name_bytes + struct.pack('<H', self.new_bag_idx)


@dataclass
class SF2Preset:
    """SF2 preset header (phdr)"""
    name: str
    preset: int  # program number
    bank: int
    bag_idx: int
    library: int
    genre: int
    morphology: int
    
    # Runtime
    new_bag_idx: int = 0
    
    @classmethod
    def from_bytes(cls, data: bytes) -> 'SF2Preset':
        name = data[0:20].rstrip(b'\x00').decode('latin-1')
        values = struct.unpack('<HHHLLL', data[20:38])
        return cls(name, *values)
    
    def to_bytes(self) -> bytes:
        name_bytes = self.name.encode('latin-1')[:20].ljust(20, b'\x00')
        return name_bytes + struct.pack(
            '<HHHLLL',
            self.preset, self.bank, self.new_bag_idx,
            self.library, self.genre, self.morphology
        )


# =============================================================================
# SF2 Parser
# =============================================================================

@dataclass
class SF2File:
    """Parsed SF2 file"""
    info_chunks: dict = field(default_factory=dict)
    sample_data: bytes = b''
    sample_data_24: bytes = b''  # Optional 24-bit extension
    
    presets: list = field(default_factory=list)
    preset_bags: list = field(default_factory=list)
    preset_mods: list = field(default_factory=list)
    preset_gens: list = field(default_factory=list)
    
    instruments: list = field(default_factory=list)
    inst_bags: list = field(default_factory=list)
    inst_mods: list = field(default_factory=list)
    inst_gens: list = field(default_factory=list)
    
    samples: list = field(default_factory=list)


def read_chunk_header(f: BinaryIO) -> tuple[str, int]:
    """Read a RIFF chunk header, returns (id, size)"""
    chunk_id = f.read(4).decode('ascii')
    size = struct.unpack('<I', f.read(4))[0]
    return chunk_id, size


def parse_sf2(filepath: Path) -> SF2File:
    """Parse an SF2 file"""
    sf2 = SF2File()
    
    with open(filepath, 'rb') as f:
        # RIFF header
        riff_id, riff_size = read_chunk_header(f)
        assert riff_id == 'RIFF', f"Not a RIFF file: {riff_id}"
        
        form_type = f.read(4).decode('ascii')
        assert form_type == 'sfbk', f"Not a soundfont: {form_type}"
        
        end_pos = f.tell() + riff_size - 4
        
        while f.tell() < end_pos:
            chunk_id, chunk_size = read_chunk_header(f)
            chunk_start = f.tell()
            
            if chunk_id == 'LIST':
                list_type = f.read(4).decode('ascii')
                list_end = chunk_start + chunk_size
                
                if list_type == 'INFO':
                    # Parse INFO chunks
                    while f.tell() < list_end:
                        info_id, info_size = read_chunk_header(f)
                        info_data = f.read(info_size)
                        if info_size % 2:  # Pad byte
                            f.read(1)
                        sf2.info_chunks[info_id] = info_data
                
                elif list_type == 'sdta':
                    # Sample data
                    while f.tell() < list_end:
                        sdta_id, sdta_size = read_chunk_header(f)
                        if sdta_id == 'smpl':
                            sf2.sample_data = f.read(sdta_size)
                        elif sdta_id == 'sm24':
                            sf2.sample_data_24 = f.read(sdta_size)
                        else:
                            f.seek(sdta_size, 1)
                        if sdta_size % 2:
                            f.read(1)
                
                elif list_type == 'pdta':
                    # Preset data
                    while f.tell() < list_end:
                        pdta_id, pdta_size = read_chunk_header(f)
                        pdta_data = f.read(pdta_size)
                        if pdta_size % 2:
                            f.read(1)
                        
                        if pdta_id == 'phdr':
                            for i in range(0, len(pdta_data), 38):
                                sf2.presets.append(SF2Preset.from_bytes(pdta_data[i:i+38]))
                        elif pdta_id == 'pbag':
                            for i in range(0, len(pdta_data), 4):
                                sf2.preset_bags.append(SF2Bag.from_bytes(pdta_data[i:i+4]))
                        elif pdta_id == 'pmod':
                            for i in range(0, len(pdta_data), 10):
                                sf2.preset_mods.append(SF2Modulator.from_bytes(pdta_data[i:i+10]))
                        elif pdta_id == 'pgen':
                            for i in range(0, len(pdta_data), 4):
                                sf2.preset_gens.append(SF2Generator.from_bytes(pdta_data[i:i+4]))
                        elif pdta_id == 'inst':
                            for i in range(0, len(pdta_data), 22):
                                sf2.instruments.append(SF2Inst.from_bytes(pdta_data[i:i+22]))
                        elif pdta_id == 'ibag':
                            for i in range(0, len(pdta_data), 4):
                                sf2.inst_bags.append(SF2Bag.from_bytes(pdta_data[i:i+4]))
                        elif pdta_id == 'imod':
                            for i in range(0, len(pdta_data), 10):
                                sf2.inst_mods.append(SF2Modulator.from_bytes(pdta_data[i:i+10]))
                        elif pdta_id == 'igen':
                            for i in range(0, len(pdta_data), 4):
                                sf2.inst_gens.append(SF2Generator.from_bytes(pdta_data[i:i+4]))
                        elif pdta_id == 'shdr':
                            for i in range(0, len(pdta_data), 46):
                                sf2.samples.append(SF2Sample.from_bytes(pdta_data[i:i+46]))
            else:
                f.seek(chunk_size, 1)
                if chunk_size % 2:
                    f.read(1)
    
    return sf2


# =============================================================================
# SF2 Subsetting
# =============================================================================

# Generator types that reference instruments or samples
GEN_INSTRUMENT = 41
GEN_SAMPLE_ID = 53
GEN_KEY_RANGE = 43
GEN_VEL_RANGE = 44


def subset_sf2(sf2: SF2File, needed_presets: set[tuple[int, int]], verbose: bool = False) -> SF2File:
    """
    Create a new SF2 containing only the specified presets.
    needed_presets is a set of (bank, program) tuples.
    """
    out = SF2File()
    out.info_chunks = sf2.info_chunks.copy()
    
    # Note: We keep the original name - modifying INFO chunks can cause
    # compatibility issues with some SF2 parsers
    
    # Find which presets we need (excluding terminal EOP)
    kept_presets = []
    for preset in sf2.presets[:-1]:  # Skip terminal
        if (preset.bank, preset.preset) in needed_presets:
            kept_presets.append(preset)
            if verbose:
                print(f"  Keeping preset: {preset.name} (bank {preset.bank}, prog {preset.preset})")
    
    if not kept_presets:
        print("Warning: No matching presets found!", file=sys.stderr)
        return out
    
    # Find which instruments are used by these presets
    kept_inst_indices = set()
    for preset in kept_presets:
        bag_start = preset.bag_idx
        bag_end = sf2.presets[sf2.presets.index(preset) + 1].bag_idx
        
        for bag_idx in range(bag_start, bag_end):
            bag = sf2.preset_bags[bag_idx]
            gen_start = bag.gen_idx
            gen_end = sf2.preset_bags[bag_idx + 1].gen_idx if bag_idx + 1 < len(sf2.preset_bags) else len(sf2.preset_gens)
            
            for gen_idx in range(gen_start, gen_end):
                gen = sf2.preset_gens[gen_idx]
                if gen.oper == GEN_INSTRUMENT:
                    kept_inst_indices.add(gen.amount)
    
    if verbose:
        print(f"  Instruments needed: {len(kept_inst_indices)}")
    
    # Find which samples are used by these instruments
    kept_sample_indices = set()
    for inst_idx in kept_inst_indices:
        inst = sf2.instruments[inst_idx]
        bag_start = inst.bag_idx
        bag_end = sf2.instruments[inst_idx + 1].bag_idx if inst_idx + 1 < len(sf2.instruments) else len(sf2.inst_bags)
        
        for bag_idx in range(bag_start, bag_end):
            bag = sf2.inst_bags[bag_idx]
            gen_start = bag.gen_idx
            gen_end = sf2.inst_bags[bag_idx + 1].gen_idx if bag_idx + 1 < len(sf2.inst_bags) else len(sf2.inst_gens)
            
            for gen_idx in range(gen_start, gen_end):
                gen = sf2.inst_gens[gen_idx]
                if gen.oper == GEN_SAMPLE_ID:
                    sample_idx = gen.amount
                    kept_sample_indices.add(sample_idx)
                    # Also keep linked samples (for stereo)
                    sample = sf2.samples[sample_idx]
                    if sample.sample_link and sample.sample_link < len(sf2.samples):
                        kept_sample_indices.add(sample.sample_link)
    
    if verbose:
        print(f"  Samples needed: {len(kept_sample_indices)} / {len(sf2.samples) - 1}")
    
    # Build new sample list and sample data
    new_sample_data = bytearray()
    sample_index_map = {}  # old index -> new index
    
    sorted_sample_indices = sorted(kept_sample_indices)
    for new_idx, old_idx in enumerate(sorted_sample_indices):
        sample = sf2.samples[old_idx]
        sample.new_index = new_idx
        sample_index_map[old_idx] = new_idx
        
        # Copy sample data (16-bit samples = 2 bytes per sample)
        sample.new_start = len(new_sample_data) // 2
        sample_bytes = sf2.sample_data[sample.start * 2:sample.end * 2]
        new_sample_data.extend(sample_bytes)
        sample.new_end = len(new_sample_data) // 2
        
        out.samples.append(sample)
    
    # Add terminal sample
    terminal_sample = SF2Sample("EOS", 0, 0, 0, 0, 0, 0, 0, 0, 0)
    terminal_sample.new_start = len(new_sample_data) // 2
    terminal_sample.new_end = terminal_sample.new_start
    out.samples.append(terminal_sample)
    
    # Update sample links for stereo pairs
    for sample in out.samples[:-1]:
        if sample.sample_link in sample_index_map:
            sample.sample_link = sample_index_map[sample.sample_link]
        else:
            sample.sample_link = 0
    
    out.sample_data = bytes(new_sample_data)
    
    # Build new instrument list
    inst_index_map = {}  # old index -> new index
    sorted_inst_indices = sorted(kept_inst_indices)
    
    for new_idx, old_idx in enumerate(sorted_inst_indices):
        inst = sf2.instruments[old_idx]
        inst.new_index = new_idx
        inst_index_map[old_idx] = new_idx
        
        inst.new_bag_idx = len(out.inst_bags)
        out.instruments.append(inst)
        
        # Copy instrument bags and generators
        bag_start = inst.bag_idx
        bag_end = sf2.instruments[old_idx + 1].bag_idx
        
        for bag_idx in range(bag_start, bag_end):
            old_bag = sf2.inst_bags[bag_idx]
            new_bag = SF2Bag(len(out.inst_gens), len(out.inst_mods))
            out.inst_bags.append(new_bag)
            
            # Copy generators, remapping sample IDs
            gen_start = old_bag.gen_idx
            gen_end = sf2.inst_bags[bag_idx + 1].gen_idx if bag_idx + 1 < len(sf2.inst_bags) else len(sf2.inst_gens)
            
            for gen_idx in range(gen_start, gen_end):
                gen = sf2.inst_gens[gen_idx]
                new_gen = SF2Generator(gen.oper, gen.amount)
                if gen.oper == GEN_SAMPLE_ID and gen.amount in sample_index_map:
                    new_gen.amount = sample_index_map[gen.amount]
                out.inst_gens.append(new_gen)
            
            # Copy modulators
            mod_start = old_bag.mod_idx
            mod_end = sf2.inst_bags[bag_idx + 1].mod_idx if bag_idx + 1 < len(sf2.inst_bags) else len(sf2.inst_mods)
            
            for mod_idx in range(mod_start, mod_end):
                out.inst_mods.append(sf2.inst_mods[mod_idx])
    
    # Add terminal instrument
    terminal_inst = SF2Inst("EOI", 0)
    terminal_inst.new_bag_idx = len(out.inst_bags)
    out.instruments.append(terminal_inst)
    
    # Terminal bag
    out.inst_bags.append(SF2Bag(len(out.inst_gens), len(out.inst_mods)))
    out.inst_gens.append(SF2Generator(0, 0))
    out.inst_mods.append(SF2Modulator(0, 0, 0, 0, 0))
    
    # Build new preset list
    for preset in kept_presets:
        preset.new_bag_idx = len(out.preset_bags)
        out.presets.append(preset)
        
        # Copy preset bags and generators
        preset_idx = sf2.presets.index(preset)
        bag_start = preset.bag_idx
        bag_end = sf2.presets[preset_idx + 1].bag_idx
        
        for bag_idx in range(bag_start, bag_end):
            old_bag = sf2.preset_bags[bag_idx]
            new_bag = SF2Bag(len(out.preset_gens), len(out.preset_mods))
            out.preset_bags.append(new_bag)
            
            # Copy generators, remapping instrument IDs
            gen_start = old_bag.gen_idx
            gen_end = sf2.preset_bags[bag_idx + 1].gen_idx if bag_idx + 1 < len(sf2.preset_bags) else len(sf2.preset_gens)
            
            for gen_idx in range(gen_start, gen_end):
                gen = sf2.preset_gens[gen_idx]
                new_gen = SF2Generator(gen.oper, gen.amount)
                if gen.oper == GEN_INSTRUMENT and gen.amount in inst_index_map:
                    new_gen.amount = inst_index_map[gen.amount]
                out.preset_gens.append(new_gen)
            
            # Copy modulators
            mod_start = old_bag.mod_idx
            mod_end = sf2.preset_bags[bag_idx + 1].mod_idx if bag_idx + 1 < len(sf2.preset_bags) else len(sf2.preset_mods)
            
            for mod_idx in range(mod_start, mod_end):
                out.preset_mods.append(sf2.preset_mods[mod_idx])
    
    # Add terminal preset
    terminal_preset = SF2Preset("EOP", 0, 0, 0, 0, 0, 0)
    terminal_preset.new_bag_idx = len(out.preset_bags)
    out.presets.append(terminal_preset)
    
    # Terminal bag
    out.preset_bags.append(SF2Bag(len(out.preset_gens), len(out.preset_mods)))
    out.preset_gens.append(SF2Generator(0, 0))
    out.preset_mods.append(SF2Modulator(0, 0, 0, 0, 0))
    
    return out


# =============================================================================
# SF2 Writing
# =============================================================================

def write_chunk(f: BinaryIO, chunk_id: str, data: bytes):
    """Write a RIFF chunk"""
    f.write(chunk_id.encode('ascii'))
    f.write(struct.pack('<I', len(data)))
    f.write(data)
    if len(data) % 2:
        f.write(b'\x00')  # Pad to even


def write_sf2(sf2: SF2File, filepath: Path):
    """Write an SF2 file"""
    # Build chunks
    
    # INFO list
    info_data = bytearray()
    for chunk_id, chunk_bytes in sf2.info_chunks.items():
        info_data.extend(chunk_id.encode('ascii'))
        info_data.extend(struct.pack('<I', len(chunk_bytes)))
        info_data.extend(chunk_bytes)
        if len(chunk_bytes) % 2:
            info_data.append(0)
    
    # sdta list (sample data)
    sdta_data = bytearray()
    sdta_data.extend(b'smpl')
    sdta_data.extend(struct.pack('<I', len(sf2.sample_data)))
    sdta_data.extend(sf2.sample_data)
    if len(sf2.sample_data) % 2:
        sdta_data.append(0)
    
    # pdta list (preset data)
    pdta_data = bytearray()
    
    # phdr
    phdr_bytes = b''.join(p.to_bytes() for p in sf2.presets)
    pdta_data.extend(b'phdr')
    pdta_data.extend(struct.pack('<I', len(phdr_bytes)))
    pdta_data.extend(phdr_bytes)
    
    # pbag
    pbag_bytes = b''.join(b.to_bytes() for b in sf2.preset_bags)
    pdta_data.extend(b'pbag')
    pdta_data.extend(struct.pack('<I', len(pbag_bytes)))
    pdta_data.extend(pbag_bytes)
    
    # pmod
    pmod_bytes = b''.join(m.to_bytes() for m in sf2.preset_mods)
    pdta_data.extend(b'pmod')
    pdta_data.extend(struct.pack('<I', len(pmod_bytes)))
    pdta_data.extend(pmod_bytes)
    
    # pgen
    pgen_bytes = b''.join(g.to_bytes() for g in sf2.preset_gens)
    pdta_data.extend(b'pgen')
    pdta_data.extend(struct.pack('<I', len(pgen_bytes)))
    pdta_data.extend(pgen_bytes)
    
    # inst
    inst_bytes = b''.join(i.to_bytes() for i in sf2.instruments)
    pdta_data.extend(b'inst')
    pdta_data.extend(struct.pack('<I', len(inst_bytes)))
    pdta_data.extend(inst_bytes)
    
    # ibag
    ibag_bytes = b''.join(b.to_bytes() for b in sf2.inst_bags)
    pdta_data.extend(b'ibag')
    pdta_data.extend(struct.pack('<I', len(ibag_bytes)))
    pdta_data.extend(ibag_bytes)
    
    # imod
    imod_bytes = b''.join(m.to_bytes() for m in sf2.inst_mods)
    pdta_data.extend(b'imod')
    pdta_data.extend(struct.pack('<I', len(imod_bytes)))
    pdta_data.extend(imod_bytes)
    
    # igen
    igen_bytes = b''.join(g.to_bytes() for g in sf2.inst_gens)
    pdta_data.extend(b'igen')
    pdta_data.extend(struct.pack('<I', len(igen_bytes)))
    pdta_data.extend(igen_bytes)
    
    # shdr
    shdr_bytes = b''.join(s.to_bytes() for s in sf2.samples)
    pdta_data.extend(b'shdr')
    pdta_data.extend(struct.pack('<I', len(shdr_bytes)))
    pdta_data.extend(shdr_bytes)
    
    # Build LIST chunks
    info_list = b'INFO' + bytes(info_data)
    sdta_list = b'sdta' + bytes(sdta_data)
    pdta_list = b'pdta' + bytes(pdta_data)
    
    # Calculate total size
    total_size = 4  # 'sfbk'
    total_size += 8 + len(info_list)  # LIST + info
    if len(info_list) % 2:
        total_size += 1
    total_size += 8 + len(sdta_list)  # LIST + sdta
    if len(sdta_list) % 2:
        total_size += 1
    total_size += 8 + len(pdta_list)  # LIST + pdta
    if len(pdta_list) % 2:
        total_size += 1
    
    # Write file
    with open(filepath, 'wb') as f:
        f.write(b'RIFF')
        f.write(struct.pack('<I', total_size))
        f.write(b'sfbk')
        
        write_chunk(f, 'LIST', info_list)
        write_chunk(f, 'LIST', sdta_list)
        write_chunk(f, 'LIST', pdta_list)


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Strip unused presets from a SoundFont file',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('-i', '--input', default='assets/sounds/soundfont.sf2',
                        help='Input soundfont file')
    parser.add_argument('-o', '--output', default='assets/sounds/soundfont-stripped.sf2',
                        help='Output soundfont file')
    parser.add_argument('-m', '--midi-dir', default='assets/music',
                        help='Directory to scan for MIDI files')
    parser.add_argument('-e', '--extra', action='append', default=[],
                        help='Extra preset to include (format: bank:program)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Verbose output')
    parser.add_argument('--dry-run', action='store_true',
                        help='Show what would be done without writing')
    
    args = parser.parse_args()
    
    input_path = Path(args.input)
    output_path = Path(args.output)
    midi_dir = Path(args.midi_dir)
    
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}", file=sys.stderr)
        return 1
    
    if not midi_dir.exists():
        print(f"Error: MIDI directory not found: {midi_dir}", file=sys.stderr)
        return 1
    
    # Scan MIDI files
    print(f"Scanning MIDI files in {midi_dir}...")
    needed = scan_midi_directory(midi_dir, args.verbose)
    
    # Add extra presets
    for extra in args.extra:
        try:
            bank, program = map(int, extra.split(':'))
            needed.add((bank, program))
            if args.verbose:
                print(f"  Adding extra preset: bank {bank}, program {program}")
        except ValueError:
            print(f"Warning: Invalid extra preset format: {extra}", file=sys.stderr)
    
    print(f"Presets needed: {sorted(needed)}")
    
    # Parse input
    print(f"Parsing {input_path}...")
    sf2 = parse_sf2(input_path)
    print(f"  {len(sf2.presets) - 1} presets, {len(sf2.samples) - 1} samples")
    print(f"  Sample data: {len(sf2.sample_data) / 1024 / 1024:.1f} MB")
    
    # Subset
    print("Creating subset...")
    out = subset_sf2(sf2, needed, args.verbose)
    print(f"  {len(out.presets) - 1} presets, {len(out.samples) - 1} samples")
    print(f"  Sample data: {len(out.sample_data) / 1024 / 1024:.1f} MB")
    
    if args.dry_run:
        print("Dry run - not writing output")
        return 0
    
    # Write output
    print(f"Writing {output_path}...")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    write_sf2(out, output_path)
    
    # Report sizes
    input_size = input_path.stat().st_size
    output_size = output_path.stat().st_size
    reduction = (1 - output_size / input_size) * 100
    
    print(f"Done!")
    print(f"  Input:  {input_size / 1024 / 1024:.1f} MB")
    print(f"  Output: {output_size / 1024 / 1024:.1f} MB")
    print(f"  Reduction: {reduction:.1f}%")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
