import struct
import shutil
import os

TRADE_ESP = "Data/FollowerUnboundTrade.esp"

def make_subrecord(sig, data):
    return sig.encode('latin1') + struct.pack('<H', len(data)) + data

def make_record(sig, flags, formid, subrecords_bytes):
    # record header: 4 sig, 4 data_size, 4 flags, 4 formid, 4 vc_info, 2 version, 2 vc2
    hdr = sig.encode('latin1') + struct.pack('<III', len(subrecords_bytes), flags, formid) + b'\x00\x00\x00\x00\x2c\x00\x00\x00'
    return hdr + subrecords_bytes

def make_fact_record(edid, formid):
    edid_bytes = edid.encode('latin1') + b'\x00'
    subs = make_subrecord('EDID', edid_bytes)
    subs += make_subrecord('DATA', struct.pack('<I', 1)) # flags
    return make_record('FACT', 0, formid, subs)

def make_quest_record(edid, full_name, formid, faction_formid, package_formid):
    subs = make_subrecord('EDID', edid.encode('latin1') + b'\x00')
    subs += make_subrecord('FULL', full_name.encode('latin1') + b'\x00')
    subs += make_subrecord('DNAM', b'\x00\x002\xff\x00\x00\x00\x00\x00\x00\x00\x00')
    subs += make_subrecord('NEXT', b'')
    subs += make_subrecord('ANAM', struct.pack('<I', 1)) # 1 alias
    
    # Alias 0
    alias_bytes = make_subrecord('ALST', struct.pack('<I', 0))
    alias_bytes += make_subrecord('ALID', b'Follower\x00')
    alias_bytes += make_subrecord('FNAM', b'\x22\x22\x00\x00') # flags
    
    # CTDA condition: GetInFaction(faction_formid) == 1.0
    ctda = struct.pack('<IfIIIIII', 0, 1.0, 71, faction_formid, 0, 0, 0, 0xFFFFFFFF)
    alias_bytes += make_subrecord('CTDA', ctda)
    
    # ALPC package: PlayerFollowerPackage (0x0005C84B)
    alias_bytes += make_subrecord('ALPC', struct.pack('<I', package_formid))
    alias_bytes += make_subrecord('VTCK', struct.pack('<I', 0))
    alias_bytes += make_subrecord('ALED', b'')
    
    subs += alias_bytes
    return make_record('QUST', 0, formid, subs)

def generate():
    with open(TRADE_ESP, 'rb') as f:
        data = f.read()

    hdr_sz = struct.unpack('<I', data[4:8])[0]
    tes4 = data[:24+hdr_sz]
    
    p = len(tes4)
    groups = []
    while p < len(data):
        sig = data[p:p+4]
        sz = struct.unpack('<I', data[p+4:p+8])[0]
        label = data[p+8:p+12]
        gdata = data[p:p+sz]
        groups.append((sig, label, gdata))
        p += sz

    # 10 new factions: 0x0100083D .. 0x01000846
    new_facts = []
    for i in range(10):
        fid = 0x0100083D + i
        edid = f"FU_FollowerSlotFaction{i+1}"
        new_facts.append(make_fact_record(edid, fid))
        
    # 10 new quests: 0x01000847 .. 0x01000850
    new_quests = []
    for i in range(10):
        fid = 0x01000847 + i
        fact_fid = 0x0100083D + i
        edid = f"FU_FollowerSlotQuest{i+1}"
        full = f"Follower Unbound Slot {i+1} Quest"
        new_quests.append(make_quest_record(edid, full, fid, fact_fid, 0x0005C84B))

    new_groups = []
    for sig, label, gdata in groups:
        if label == b'FACT':
            inner = gdata[24:] + b''.join(new_facts)
            new_sz = 24 + len(inner)
            new_grup_hdr = gdata[:4] + struct.pack('<I', new_sz) + gdata[8:24]
            new_groups.append(new_grup_hdr + inner)
        elif label == b'QUST':
            inner = gdata[24:] + b''.join(new_quests)
            new_sz = 24 + len(inner)
            new_grup_hdr = gdata[:4] + struct.pack('<I', new_sz) + gdata[8:24]
            new_groups.append(new_grup_hdr + inner)
        else:
            new_groups.append(gdata)

    out = tes4 + b''.join(new_groups)
    
    hedr_p = tes4.find(b'HEDR')
    if hedr_p != -1:
        out = bytearray(out)
        struct.pack_into('<I', out, hedr_p + 6 + 8, 0x851)
        out = bytes(out)

    with open(TRADE_ESP, 'wb') as f:
        f.write(out)

    print(f"Successfully updated {TRADE_ESP}, new size: {len(out)} bytes!")

if __name__ == '__main__':
    generate()
