import sys
import struct
import ctypes
from ctypes import wintypes

# Load Kernel32 with error handling
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

# Constants
RT_ICON = 3
RT_GROUP_ICON = 14

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

def inject_ico_to_exe(exe_path, ico_path, group_icon_id=120):
    print(f"Injecting {ico_path} into {exe_path} as Resource ID {group_icon_id}...")
    
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
        # Use simpler sequential ID: 1, 2, 3...
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
        
    hUpdate = kernel32.BeginUpdateResourceW(exe_path, False)
    if not hUpdate:
        err = kernel32.GetLastError()
        print(f"Error: BeginUpdateResourceW failed. Error: {err}")
        return False
        
    try:
        # 1. Write RT_ICON resources
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
            print(f"Wrote RT_ICON {entry['res_id']} (size: {entry['width']}x{entry['height']})")
            
        # 2. Write RT_GROUP_ICON resource
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
        ret = kernel32.UpdateResourceW(
            hUpdate,
            MAKEINTRESOURCEW(RT_GROUP_ICON),
            MAKEINTRESOURCEW(group_icon_id),
            1033,
            ctypes.cast(grp_buffer, wintypes.LPVOID),
            len(grp_data)
        )
        if not ret:
            err = kernel32.GetLastError()
            raise Exception(f"Failed to write RT_GROUP_ICON {group_icon_id}. Error: {err}")
        print(f"Wrote RT_GROUP_ICON {group_icon_id}")
        
        # End update and write changes to disk
        ret = kernel32.EndUpdateResourceW(hUpdate, False)
        if not ret:
            err = kernel32.GetLastError()
            print(f"Error: EndUpdateResourceW failed. Error: {err}")
            return False
            
        print("Successfully injected icon into executable.")
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
