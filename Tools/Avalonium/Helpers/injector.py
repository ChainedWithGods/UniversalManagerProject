#!/usr/bin/env python3
import ctypes
import ctypes.wintypes as wintypes
import sys
import os

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
advapi32 = ctypes.WinDLL('advapi32', use_last_error=True)

PROCESS_ALL_ACCESS = 0x1F0FFF

PROCESS_CREATE_THREAD = 0x0002
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_OPERATION = 0x0008
PROCESS_VM_READ = 0x0010
PROCESS_VM_WRITE = 0x0020

INJECT_ACCESS = (
    PROCESS_CREATE_THREAD |
    PROCESS_QUERY_INFORMATION |
    PROCESS_VM_OPERATION |
    PROCESS_VM_WRITE |
    PROCESS_VM_READ
)
MEM_COMMIT = 0x1000
MEM_RESERVE = 0x2000
MEM_RELEASE = 0x8000
PAGE_READWRITE = 0x04
EVENT_MODIFY_STATE = 0x0002
INFINITE = 0xFFFFFFFF
TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE = 0x00000008

class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [
        ('dwSize', wintypes.DWORD),
        ('cntUsage', wintypes.DWORD),
        ('th32ProcessID', wintypes.DWORD),
        ('th32DefaultHeapID', ctypes.POINTER(ctypes.c_ulong)),
        ('th32ModuleID', wintypes.DWORD),
        ('cntThreads', wintypes.DWORD),
        ('th32ParentProcessID', wintypes.DWORD),
        ('pcPriClassBase', ctypes.c_long),
        ('dwFlags', wintypes.DWORD),
        ('szExeFile', ctypes.c_char * 260),
    ]

class MODULEENTRY32(ctypes.Structure):
    _fields_ = [
        ('dwSize', wintypes.DWORD),
        ('th32ModuleID', wintypes.DWORD),
        ('th32ProcessID', wintypes.DWORD),
        ('GlblcntUsage', wintypes.DWORD),
        ('ProccntUsage', wintypes.DWORD),
        ('modBaseAddr', ctypes.POINTER(ctypes.c_byte)),
        ('modBaseSize', wintypes.DWORD),
        ('hModule', wintypes.HMODULE),
        ('szModule', ctypes.c_char * 256),
        ('szExePath', ctypes.c_char * 260),
    ]

OpenProcess = kernel32.OpenProcess
OpenProcess.restype = wintypes.HANDLE
OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]

CloseHandle = kernel32.CloseHandle
CloseHandle.restype = wintypes.BOOL
CloseHandle.argtypes = [wintypes.HANDLE]

VirtualAllocEx = kernel32.VirtualAllocEx
VirtualAllocEx.restype = wintypes.LPVOID
VirtualAllocEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD]

VirtualFreeEx = kernel32.VirtualFreeEx
VirtualFreeEx.restype = wintypes.BOOL
VirtualFreeEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.DWORD]

WriteProcessMemory = kernel32.WriteProcessMemory
WriteProcessMemory.restype = wintypes.BOOL
WriteProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPVOID, wintypes.LPCVOID, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]

CreateRemoteThread = kernel32.CreateRemoteThread
CreateRemoteThread.restype = wintypes.HANDLE
CreateRemoteThread.argtypes = [wintypes.HANDLE, wintypes.LPVOID, ctypes.c_size_t, wintypes.LPVOID, wintypes.LPVOID, wintypes.DWORD, wintypes.LPDWORD]

WaitForSingleObject = kernel32.WaitForSingleObject
WaitForSingleObject.restype = wintypes.DWORD
WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]

GetModuleHandleA = kernel32.GetModuleHandleA
GetModuleHandleA.restype = wintypes.HMODULE
GetModuleHandleA.argtypes = [wintypes.LPCSTR]

GetProcAddress = kernel32.GetProcAddress
GetProcAddress.restype = wintypes.LPVOID
GetProcAddress.argtypes = [wintypes.HMODULE, wintypes.LPCSTR]

CreateToolhelp32Snapshot = kernel32.CreateToolhelp32Snapshot
CreateToolhelp32Snapshot.restype = wintypes.HANDLE
CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]

Process32First = kernel32.Process32First
Process32First.restype = wintypes.BOOL
Process32First.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32)]

Process32Next = kernel32.Process32Next
Process32Next.restype = wintypes.BOOL
Process32Next.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32)]

Module32First = kernel32.Module32First
Module32First.restype = wintypes.BOOL
Module32First.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32)]

Module32Next = kernel32.Module32Next
Module32Next.restype = wintypes.BOOL
Module32Next.argtypes = [wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32)]

OpenEventA = kernel32.OpenEventA
OpenEventA.restype = wintypes.HANDLE
OpenEventA.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCSTR]

SetEvent = kernel32.SetEvent
SetEvent.restype = wintypes.BOOL
SetEvent.argtypes = [wintypes.HANDLE]

Sleep = kernel32.Sleep
Sleep.argtypes = [wintypes.DWORD]

FormatMessageW = kernel32.FormatMessageW
FormatMessageW.restype = wintypes.DWORD
FormatMessageW.argtypes = [
    wintypes.DWORD,
    wintypes.LPCVOID,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.LPWSTR,
    wintypes.DWORD,
    wintypes.LPVOID,
]

LocalFree = kernel32.LocalFree
LocalFree.restype = wintypes.HLOCAL
LocalFree.argtypes = [wintypes.HLOCAL]

GetCurrentProcess = kernel32.GetCurrentProcess
GetCurrentProcess.restype = wintypes.HANDLE
GetCurrentProcess.argtypes = []

OpenProcessToken = advapi32.OpenProcessToken
OpenProcessToken.restype = wintypes.BOOL
OpenProcessToken.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(wintypes.HANDLE)]

LookupPrivilegeValueW = advapi32.LookupPrivilegeValueW
LookupPrivilegeValueW.restype = wintypes.BOOL
LookupPrivilegeValueW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_longlong)]

AdjustTokenPrivileges = advapi32.AdjustTokenPrivileges
AdjustTokenPrivileges.restype = wintypes.BOOL
AdjustTokenPrivileges.argtypes = [
    wintypes.HANDLE,
    wintypes.BOOL,
    wintypes.LPVOID,
    wintypes.DWORD,
    wintypes.LPVOID,
    ctypes.POINTER(wintypes.DWORD),
]

TOKEN_ADJUST_PRIVILEGES = 0x0020
TOKEN_QUERY = 0x0008
SE_PRIVILEGE_ENABLED = 0x00000002

FORMAT_MESSAGE_ALLOCATE_BUFFER = 0x00000100
FORMAT_MESSAGE_FROM_SYSTEM = 0x00001000
FORMAT_MESSAGE_IGNORE_INSERTS = 0x00000200


class LUID_AND_ATTRIBUTES(ctypes.Structure):
    _fields_ = [
        ("Luid", ctypes.c_longlong),
        ("Attributes", wintypes.DWORD),
    ]


class TOKEN_PRIVILEGES(ctypes.Structure):
    _fields_ = [
        ("PrivilegeCount", wintypes.DWORD),
        ("Privileges", LUID_AND_ATTRIBUTES * 1),
    ]


def format_win_error(err):
    if not err:
        return "(no error)"

    buf = wintypes.LPWSTR()
    flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS
    length = FormatMessageW(flags, None, err, 0, ctypes.cast(ctypes.byref(buf), wintypes.LPWSTR), 0, None)
    if not length:
        return "(unable to format error)"
    try:
        return buf.value.strip()
    finally:
        LocalFree(buf)


def enable_debug_privilege():
    token = wintypes.HANDLE()
    if not OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, ctypes.byref(token)):
        return False

    try:
        luid = ctypes.c_longlong()
        if not LookupPrivilegeValueW(None, "SeDebugPrivilege", ctypes.byref(luid)):
            return False

        tp = TOKEN_PRIVILEGES()
        tp.PrivilegeCount = 1
        tp.Privileges[0].Luid = luid.value
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED

        if not AdjustTokenPrivileges(token, False, ctypes.byref(tp), 0, None, None):
            return False


        return ctypes.get_last_error() == 0
    finally:
        CloseHandle(token)


def find_process_by_name(name):
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snap == wintypes.HANDLE(-1).value:
        return 0
    
    pe = PROCESSENTRY32()
    pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
    
    if Process32First(snap, ctypes.byref(pe)):
        while True:
            if pe.szExeFile.decode('utf-8', errors='ignore').lower() == name.lower():
                CloseHandle(snap)
                return pe.th32ProcessID
            if not Process32Next(snap, ctypes.byref(pe)):
                break
    
    CloseHandle(snap)
    return 0


def unload_previous(pid):
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)
    if snap == wintypes.HANDLE(-1).value:
        return True

    me = MODULEENTRY32()
    me.dwSize = ctypes.sizeof(MODULEENTRY32)

    loaded_modules = []

    if Module32First(snap, ctypes.byref(me)):
        while True:
            mod_name = me.szModule.decode('utf-8', errors='ignore')
            if 'UM_' in mod_name or 'Avalonium' in mod_name or 'UniversalManager' in mod_name:
                print(f'Found: {mod_name}')
                loaded_modules.append(me.hModule)
            if not Module32Next(snap, ctypes.byref(me)):
                break

    CloseHandle(snap)

    if not loaded_modules:
        return True
        
    print('Avalonium is already loaded in this process; restart the game to re-inject.')
    return False


def inject(pid, dll_path):
   
    enable_debug_privilege()

    proc = OpenProcess(INJECT_ACCESS, False, pid)
    if not proc:
        err = ctypes.get_last_error()
        print(f'Open failed: {err} ({format_win_error(err)})')
        return False
    
    path_bytes = dll_path.encode('utf-8') + b'\x00'
    path_len = len(path_bytes)
    
    mem = VirtualAllocEx(proc, None, path_len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
    if not mem:
        print(f'Alloc failed: {ctypes.get_last_error()}')
        CloseHandle(proc)
        return False
    
    written = ctypes.c_size_t()
    if not WriteProcessMemory(proc, mem, path_bytes, path_len, ctypes.byref(written)):
        print(f'Write failed: {ctypes.get_last_error()}')
        VirtualFreeEx(proc, mem, 0, MEM_RELEASE)
        CloseHandle(proc)
        return False
    
    k32 = GetModuleHandleA(b'kernel32.dll')
    load_lib = GetProcAddress(k32, b'LoadLibraryA')
    
    if not load_lib:
        print('GetAddr failed')
        VirtualFreeEx(proc, mem, 0, MEM_RELEASE)
        CloseHandle(proc)
        return False
    
    thread = CreateRemoteThread(proc, None, 0, load_lib, mem, 0, None)
    if not thread:
        print(f'Thread failed: {ctypes.get_last_error()}')
        VirtualFreeEx(proc, mem, 0, MEM_RELEASE)
        CloseHandle(proc)
        return False
    
    WaitForSingleObject(thread, INFINITE)
    
    VirtualFreeEx(proc, mem, 0, MEM_RELEASE)
    CloseHandle(thread)
    CloseHandle(proc)
    
    return True


def main():
    if len(sys.argv) != 3:
        print('Usage: python injector.py <PID|ProcessName> <DLL>')
        print('Ex: python injector.py 1234 path.dll')
        return 1
    
    target = sys.argv[1]
    dll_path = os.path.abspath(sys.argv[2])
    
    if not os.path.exists(dll_path):
        print('DLL not found')
        return 1
    
    try:
        pid = int(target)
    except ValueError:
        pid = find_process_by_name(target)
        if pid == 0:
            print(f'Process not found: {target}')
            return 1
    
    print(f'PID: {pid}')
    print(f'DLL: {dll_path}')
    
    if not unload_previous(pid):
        print('Injection aborted.')
        return 2

    print('Loading...')

    if inject(pid, dll_path):
        print('Done!')
        return 0
    else:
        print('Failed!')
        return 1


if __name__ == '__main__':
    sys.exit(main())
