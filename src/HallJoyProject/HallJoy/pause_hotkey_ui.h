#pragma once
// Included after GlobalSettingsPageState and its existing toast helpers.
// Pause/Resume shortcuts use the same shortcut system as the Block Bound Keys
// toggle: digital or analog presses, optional Ctrl/Alt/Shift/Win chords.
static void Global_StopPauseCapture(HWND hWnd, GlobalSettingsPageState* st)
{
    if(!st || st->pauseCaptureSlot<0) return;
    st->pauseCaptureSlot=-1;
    App_EndShortcutCapture(hWnd);
    CustomPageSurface_MarkDirty(hWnd,&st->surface);
}
static bool Global_SavePauseSettings(HWND hWnd,const unsigned (&shortcuts)[3],bool separate)
{
    unsigned old[3]{};
    for(unsigned i=0;i<3;++i) old[i]=Settings_GetPauseShortcut(i);
    const bool oldSeparate=Settings_GetPauseSeparate();
    for(unsigned i=0;i<3;++i) Settings_SetPauseShortcut(i,shortcuts[i]);
    Settings_SetPauseSeparate(separate);
    if(SettingsIni_SaveWindow(AppPaths_SettingsIni().c_str())) return true;
    for(unsigned i=0;i<3;++i) Settings_SetPauseShortcut(i,old[i]);
    Settings_SetPauseSeparate(oldSeparate);
    MessageBoxW(hWnd,L"Could not save shortcuts. The previous settings are unchanged.",L"HallJoy",MB_OK|MB_ICONWARNING);
    return false;
}
static bool Global_SavePauseShortcut(HWND hWnd,unsigned slot,unsigned shortcut)
{
    unsigned shortcuts[3]{Settings_GetPauseShortcut(0),Settings_GetPauseShortcut(1),Settings_GetPauseShortcut(2)};
    shortcuts[slot]=shortcut;
    return Global_SavePauseSettings(hWnd,shortcuts,Settings_GetPauseSeparate());
}
static std::wstring Global_PauseKeyText(unsigned shortcut)
{
    return ShortcutText(shortcut);
}
// Completion of an explicit capture (engine message) for the active slot.
static void Global_OnShortcutCaptured(HWND hWnd,GlobalSettingsPageState* st,unsigned result)
{
    if(!st || st->pauseCaptureSlot<0) return;
    const unsigned slot=static_cast<unsigned>(st->pauseCaptureSlot);
    Global_StopPauseCapture(hWnd,st);
    if(result && result!=halljoy::shortcuts::kCaptureCancelled) {
        const DWORD error=App_ValidatePauseShortcut(slot,result);
        if(error==ERROR_ALREADY_ASSIGNED)
            GlobalToast_ShowNearCursor(hWnd,st,L"This shortcut is already used by another command.");
        else if(error)
            GlobalToast_ShowNearCursor(hWnd,st,L"This key cannot be used as a shortcut.");
        else Global_SavePauseShortcut(hWnd,slot,result);
    }
    CustomPageSurface_MarkDirty(hWnd,&st->surface);
}
static bool Global_PauseCommand(HWND hWnd,GlobalSettingsPageState* st,WPARAM command)
{
    if(HIWORD(command)!=BN_CLICKED || LOWORD(command)<GLOB_ID_PAUSE_SEPARATE || LOWORD(command)>=GLOB_ID_PAUSE_CLEAR+3) return false;
    const auto id=LOWORD(command);
    const int previous=st->pauseCaptureSlot;
    Global_StopPauseCapture(hWnd,st);
    if(id==GLOB_ID_PAUSE_SEPARATE) {
        unsigned shortcuts[3]{Settings_GetPauseShortcut(0),Settings_GetPauseShortcut(1),Settings_GetPauseShortcut(2)};
        Global_SavePauseSettings(hWnd,shortcuts,!Settings_GetPauseSeparate());
        Global_Layout(hWnd,st);
    } else if(id>=GLOB_ID_PAUSE_CLEAR) {
        const unsigned slot=id-GLOB_ID_PAUSE_CLEAR;
        // While capturing, the same button reads "Cancel".
        if(previous!=static_cast<int>(slot)) Global_SavePauseShortcut(hWnd,slot,0);
    } else if(previous!=id-GLOB_ID_PAUSE_KEY) {
        st->pauseCaptureSlot=id-GLOB_ID_PAUSE_KEY;
        SetFocus(hWnd);
        App_BeginShortcutCapture(hWnd);
    }
    CustomPageSurface_MarkDirty(hWnd,&st->surface);
    return true;
}
