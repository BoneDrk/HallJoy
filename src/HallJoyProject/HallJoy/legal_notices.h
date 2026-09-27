#pragma once
#include <windows.h>
#include <string>
#include "Resource.h"

inline std::wstring HallJoyLegalText(HINSTANCE instance, int id)
{
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!resource) return {};
    HGLOBAL loaded = LoadResource(instance, resource);
    const auto* bytes = static_cast<const char*>(LockResource(loaded));
    const int size = static_cast<int>(SizeofResource(instance, resource));
    if (!bytes || size <= 0) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, size, nullptr, 0);
    if (count <= 0) return {};
    std::wstring text(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, size, text.data(), count);
    std::wstring normalized;
    for (wchar_t c : text) {
        if (c == L'\r') continue;
        if (c == L'\n') normalized += L'\r';
        normalized += c;
    }
    return normalized;
}

inline INT_PTR CALLBACK HallJoyLegalDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM)
{
    if (message == WM_INITDIALOG) {
        auto license = HallJoyLegalText(GetModuleHandleW(nullptr), IDR_HALLJOY_LICENSE);
        auto notices = HallJoyLegalText(GetModuleHandleW(nullptr), IDR_THIRD_PARTY_NOTICES);
        std::wstring text = license.empty() || notices.empty()
            ? L"License resources could not be loaded. See https://github.com/PashOK7/HallJoy"
            : notices + L"\r\n\r\nHallJoy license\r\n\r\n" + license;
        SendDlgItemMessageW(window, IDC_LICENSE_TEXT, EM_SETLIMITTEXT, 0x7ffffffe, 0);
        SetDlgItemTextW(window, IDC_LICENSE_TEXT, text.c_str());
        return TRUE;
    }
    if ((message == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)) || message == WM_CLOSE) {
        EndDialog(window, IDOK);
        return TRUE;
    }
    return FALSE;
}

inline void HallJoyShowLicenses(HWND owner)
{
    if (DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_LICENSES), owner,
        HallJoyLegalDialogProc, 0) == -1)
        MessageBoxW(owner, L"Could not open license information.", L"HallJoy", MB_OK | MB_ICONERROR);
}
