"""Read-only Windows PE resource check; does not execute the candidate application."""
import ctypes
from ctypes import wintypes as w
from pathlib import Path
import sys


def verify(exe, root):
    api = ctypes.WinDLL('kernel32', use_last_error=True)
    api.LoadLibraryExW.argtypes = [w.LPCWSTR, w.HANDLE, w.DWORD]
    api.LoadLibraryExW.restype = w.HMODULE
    api.FindResourceW.argtypes = [w.HMODULE, ctypes.c_void_p, ctypes.c_void_p]
    api.FindResourceW.restype = w.HANDLE
    api.LoadResource.argtypes = [w.HMODULE, w.HANDLE]
    api.LoadResource.restype = w.HANDLE
    api.LockResource.argtypes = [w.HANDLE]
    api.LockResource.restype = ctypes.c_void_p
    api.SizeofResource.argtypes = [w.HMODULE, w.HANDLE]
    api.SizeofResource.restype = w.DWORD
    api.FreeLibrary.argtypes = [w.HMODULE]
    module = api.LoadLibraryExW(str(exe.resolve()), None, 0x22)  # DATAFILE | IMAGE_RESOURCE
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        for resource_id, name in [(133, 'LICENSE'), (134, 'THIRD_PARTY_NOTICES.md'),
                                  (131, 'build/runtime/universal_analog_abiv1.dll')]:
            resource = api.FindResourceW(module, resource_id, 10)
            if not resource:
                raise RuntimeError('Missing embedded ' + name)
            size = api.SizeofResource(module, resource)
            pointer = api.LockResource(api.LoadResource(module, resource))
            if not pointer or not size:
                raise RuntimeError('Unreadable embedded ' + name)
            data = ctypes.string_at(pointer, size)
            if resource_id != 131:
                data.decode('utf-8')
            if data != (root / name).read_bytes():
                raise RuntimeError('Stale or different embedded ' + name)
        if not api.FindResourceW(module, 135, 5):
            raise RuntimeError('Missing license viewer dialog')
    finally:
        api.FreeLibrary(module)
    print('Embedded ABI1, LICENSE, notices and viewer: PASS (exact source bytes)')


if __name__ == '__main__':
    verify(Path(sys.argv[1]), Path(__file__).resolve().parents[1])
