import sys
import os
import struct
import ctypes
from ctypes import wintypes

# Load Kernel32 and Shell32 with error handling
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
shell32 = ctypes.WinDLL('shell32', use_last_error=True)

# Constants
RT_ICON = 3
RT_GROUP_ICON = 14
SHCNE_ASSOCCHANGED = 0x08000000
SHCNF_IDLIST = 0

# Define prototypes
kernel32.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
kernel32.BeginUpdateResourceW.restype = wintypes.HANDLE

kernel32.UpdateResourceW.argtypes = [
    wintypes.HANDLE,
    wintypes.LPCWSTR,
    wintypes.LPCWSTR,
    wintypes.WORD,
    wintypes.LPVOID,
    wintypes.DWORD
]
kernel32.UpdateResourceW.restype = wintypes.BOOL

kernel32.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]
kernel32.EndUpdateResourceW.restype = wintypes.BOOL

def MAKEINTRESOURCEW(i):
    return ctypes.cast(ctypes.c_void_p(i), wintypes.LPCWSTR)

def get_existing_icon_ids(pe_path):
    """Parse PE resource section directly to find all existing RT_ICON IDs."""
    try:
        with open(pe_path, 'rb') as f:
            data = f.read()
        pe_off = struct.unpack('<I', data[0x3C:0x40])[0]
        num_sec = struct.unpack('<H', data[pe_off+6:pe_off+8])[0]
        opt_magic = struct.unpack('<H', data[pe_off+24:pe_off+26])[0]
        sec_start = pe_off + 24 + (240 if opt_magic == 0x20B else 224)
        rsrc_sec = None
        for i in range(num_sec):
            sec = data[sec_start + i*40 : sec_start + (i+1)*40]
            if sec[:5] == b'.rsrc':
                rsrc_sec = sec
                break
        if not rsrc_sec:
            return []
        vsize, rva, raw_size, raw_ptr = struct.unpack('<IIII', rsrc_sec[8:24])
        rsrc_data = data[raw_ptr:raw_ptr+raw_size]
        def parse_dir(off):
            named, ids = struct.unpack('<HH', rsrc_data[off+12:off+16])
            entries = []
            for i in range(named + ids):
                nid, odata = struct.unpack('<II', rsrc_data[off+16+i*8:off+24+i*8])
                entries.append((nid, bool(odata & 0x80000000), odata & 0x7FFFFFFF))
            return entries
        types = parse_dir(0)
        icon_ids = []
        for tid, is_dir, sub in types:
            if tid == RT_ICON and is_dir:
                names = parse_dir(sub)
                for nid, _, _ in names:
                    icon_ids.append(nid)
        return icon_ids
    except Exception as e:
        print(f"Warning: Failed to parse existing icons from PE: {e}")
        return []

def inject_ico_to_exe(exe_path, ico_path, primary_group_id=120):
    print(f"Injecting {ico_path} into {exe_path}...")
    
    with open(ico_path, 'rb') as f:
        ico_data = f.read()
        
    if len(ico_data) < 6:
        print("Error: Invalid ICO file size")
        return False
        
    reserved, ico_type, img_count = struct.unpack('<HHH', ico_data[:6])
    if reserved != 0 or ico_type != 1:
        print("Error: Not a valid ICO file")
        return False
        
    print(f"ICO contains {img_count} images.")
    
    # Read icon directory entries
    entries = []
    offset = 6
    for i in range(img_count):
        width, height, color_count, res, planes, bit_count, bytes_in_res, image_offset = struct.unpack(
            '<BBBBHHII', ico_data[offset:offset+16]
        )
        icon_res_id = i + 1
        entries.append({
            'width': width,
            'height': height,
            'color_count': color_count,
            'reserved': res,
            'planes': planes,
            'bit_count': bit_count,
            'bytes_in_res': bytes_in_res,
            'image_offset': image_offset,
            'res_id': icon_res_id
        })
        offset += 16
        
    existing_icon_ids = get_existing_icon_ids(exe_path)
    print(f"Existing RT_ICON IDs in PE: {existing_icon_ids}")
    
    hUpdate = kernel32.BeginUpdateResourceW(exe_path, False)
    if not hUpdate:
        err = kernel32.GetLastError()
        print(f"Error: BeginUpdateResourceW failed. Error: {err}")
        return False
        
    try:
        # 1. Write RT_ICON resources (ID: 1..img_count)
        for entry in entries:
            img_data = ico_data[entry['image_offset'] : entry['image_offset'] + entry['bytes_in_res']]
            data_buffer = ctypes.create_string_buffer(img_data)
            
            ret = kernel32.UpdateResourceW(
                hUpdate,
                MAKEINTRESOURCEW(RT_ICON),
                MAKEINTRESOURCEW(entry['res_id']),
                1033, # LANG_ENGLISH / SUBLANG_ENGLISH_US
                ctypes.cast(data_buffer, wintypes.LPVOID),
                len(img_data)
            )
            if not ret:
                err = kernel32.GetLastError()
                raise Exception(f"Failed to write RT_ICON {entry['res_id']}. Error: {err}")
            w_disp = 256 if entry['width'] == 0 else entry['width']
            h_disp = 256 if entry['height'] == 0 else entry['height']
            print(f"Wrote RT_ICON {entry['res_id']} (size: {w_disp}x{h_disp})")
            
        # 2. Safely delete existing legacy dummy RT_ICONs from bootstrap.exe
        for old_id in existing_icon_ids:
            if old_id > img_count:
                ret = kernel32.UpdateResourceW(
                    hUpdate,
                    MAKEINTRESOURCEW(RT_ICON),
                    MAKEINTRESOURCEW(old_id),
                    1033,
                    None,
                    0
                )
                print(f"Deleted legacy RT_ICON {old_id}: result={ret}")
            
        # 3. Build unified RT_GROUP_ICON payload
        grp_header = struct.pack('<HHH', 0, 1, img_count)
        grp_data = grp_header
        
        for entry in entries:
            grp_entry = struct.pack(
                '<BBBBHHIH',
                entry['width'],
                entry['height'],
                entry['color_count'],
                entry['reserved'],
                entry['planes'],
                entry['bit_count'],
                entry['bytes_in_res'],
                entry['res_id']
            )
            grp_data += grp_entry
            
        grp_buffer = ctypes.create_string_buffer(grp_data)
        
        # Inject into all standard group icon IDs:
        # 120: IDI_CEFSIMPLE
        # 121: IDI_SMALL
        # 32512: IDI_APPLICATION (standard Windows default app icon)
        target_group_ids = sorted(list({primary_group_id, 120, 121, 32512}))
        
        for gid in target_group_ids:
            ret = kernel32.UpdateResourceW(
                hUpdate,
                MAKEINTRESOURCEW(RT_GROUP_ICON),
                MAKEINTRESOURCEW(gid),
                1033,
                ctypes.cast(grp_buffer, wintypes.LPVOID),
                len(grp_data)
            )
            if not ret:
                err = kernel32.GetLastError()
                raise Exception(f"Failed to write RT_GROUP_ICON {gid}. Error: {err}")
            print(f"Wrote RT_GROUP_ICON {gid}")
            
        # End update and write changes to disk
        ret = kernel32.EndUpdateResourceW(hUpdate, False)
        if not ret:
            err = kernel32.GetLastError()
            print(f"Error: EndUpdateResourceW failed. Error: {err}")
            return False
            
        print("Successfully injected icon into executable.")
        
        # Notify Windows Shell of icon changes
        try:
            shell32.SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, None, None)
            print("Notified Windows Shell (SHChangeNotify) to refresh icon cache.")
        except Exception as e:
            print(f"Warning: SHChangeNotify failed: {e}")
            
        return True
        
    except Exception as e:
        print(f"Exception raised during icon injection: {e}")
        kernel32.EndUpdateResourceW(hUpdate, True)
        return False

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python inject_icon.py <path_to_exe> <path_to_ico> [group_icon_id]")
        sys.exit(1)
        
    exe_path = sys.argv[1]
    ico_path = sys.argv[2]
    group_id = int(sys.argv[3]) if len(sys.argv) > 3 else 120
    
    success = inject_ico_to_exe(exe_path, ico_path, group_id)
    sys.exit(0 if success else 1)
