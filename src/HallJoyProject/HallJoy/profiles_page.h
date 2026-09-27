#pragma once
#include <commdlg.h>
#include "keychron_onboard_backend.h"
#include "keychron_onboard_host_profile.h"
#include "game_profiles.h"
#include "ui_theme.h"
#include "backend.h"
#include "settings.h"
#include "keyboard_ui_state.h"
#include "keyboard_ui.h"
#include "keyboard_ui_internal.h"
#pragma comment(lib,"Comdlg32.lib")

namespace halljoy::profiles::ui {
constexpr UINT ForegroundChanged=WM_APP+480;
constexpr UINT Refresh=WM_APP+481;
constexpr UINT_PTR ForegroundTimer=480;
inline HWND page=nullptr,body=nullptr;
inline HWINEVENTHOOK hook=nullptr;
inline Session session;
inline std::wstring lastExternalExe;

inline int scroll=0;
inline bool visualPending=false;
inline std::vector<std::wstring> names;
inline std::vector<size_t> gameRows;
inline std::wstring selected;
enum Id { List=6100, Status, NameLabel, Name, Activate, Duplicate, Rename, Delete, New,
    GamesLabel, Games, AddExe, AddRunning, RemoveGame, Automatic, Resume, Undo, Details, Error };
inline HWND Control(int id) {HWND c=GetDlgItem(page,id);return c?c:GetDlgItem(body,id);}
inline std::wstring Text(int id) {wchar_t b[512]{};GetWindowTextW(Control(id),b,512);return b;}
inline std::wstring ProcessPath(DWORD pid) {
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return {};
    wchar_t b[32768]{};DWORD n=_countof(b);const bool ok=QueryFullProcessImageNameW(p,0,b,&n)!=FALSE;CloseHandle(p);
    return ok?std::wstring(b,n):std::wstring();
}
inline void ResumeVisual() {
    if(!page||!visualPending)return;
    const HWND root=GetAncestor(page,GA_ROOT);
    if(!IsWindowVisible(root)||IsIconic(root))return;
    visualPending=false;
    if(g_hPageConfig)PostMessageW(g_hPageConfig,WM_APP+121,0,0);
    if(g_hPageGlobal)PostMessageW(g_hPageGlobal,WM_APP+122,0,0);
    PostMessageW(GetParent(GetParent(page)),WM_APP_KEYBOARD_LAYOUT_CHANGED,0,0);
    if(IsWindowVisible(page))PostMessageW(page,Refresh,0,0);
}
inline void Applied() {
    Backend_SetVirtualGamepadCount(Settings_GetVirtualGamepadCount());
    Backend_SetVirtualGamepadsEnabled(Settings_GetVirtualGamepadsEnabled());
    const HWND root=GetAncestor(page,GA_ROOT);
    SetWindowTextW(root,(L"HallJoy - "+GlobalProfiles_GetActiveName()).c_str());
    SendMessageW(root,WM_APP_PROFILE_RUNTIME_APPLIED,0,0);
    PostMessageW(root,WM_APP+1,0,0);
    visualPending=true;ResumeVisual();
}
inline void Layout();
inline void Update() {
    const auto active=GlobalProfiles_GetActiveName();
    const auto mode=session.catalog.automatic?(session.manual?L"Manual override":L"Automatic"):L"Manual";
    const auto title=L"Active: "+active+L"  |  "+mode;
    SetWindowTextW(Control(Status),title.c_str());
    SetWindowTextW(Control(Error),session.error.c_str());
    ShowWindow(Control(Error),session.error.empty()?SW_HIDE:SW_SHOW);
    SetWindowTextW(GetAncestor(page,GA_ROOT),(L"HallJoy - "+active).c_str());
    std::vector<std::wstring> next;GlobalProfiles_List(next);names=std::move(next);
    if(selected.empty()||!Exists(selected))selected=active;
    SendMessageW(Control(List),WM_SETREDRAW,FALSE,0);SendMessageW(Control(List),LB_RESETCONTENT,0,0);
    int selection=0;
    for(size_t i=0;i<names.size();++i) {
        const auto label=names[i]+(Same(names[i],active)?L"  (active)":L"");
        SendMessageW(Control(List),LB_ADDSTRING,0,(LPARAM)label.c_str());
        if(Same(selected,names[i]))selection=(int)i;
    }
    SendMessageW(Control(List),LB_SETCURSEL,selection,0);SendMessageW(Control(List),WM_SETREDRAW,TRUE,0);InvalidateRect(Control(List),nullptr,FALSE);
    SetWindowTextW(Control(Name),selected.c_str());
    SendMessageW(Control(Games),LB_RESETCONTENT,0,0);gameRows.clear();
    for(size_t i=0;i<session.catalog.games.size();++i)if(Same(session.catalog.games[i].profile,selected)) {
        gameRows.push_back(i);SendMessageW(Control(Games),LB_ADDSTRING,0,(LPARAM)session.catalog.games[i].exe.c_str());
    }
    SendMessageW(Control(Automatic),BM_SETCHECK,session.catalog.automatic?BST_CHECKED:BST_UNCHECKED,0);
    EnableWindow(Control(Resume),session.catalog.automatic&&session.manual);
    EnableWindow(Control(Rename),!GlobalProfiles_IsDefault(selected));
    EnableWindow(Control(Delete),!GlobalProfiles_IsDefault(selected)&&!Same(selected,active));
    EnableWindow(Control(Undo),Same(selected,active)&&bool(session.checkpoint));
    EnableWindow(Control(RemoveGame),!gameRows.empty());
    SetWindowTextW(Control(Details),session.error.empty()?
        (Same(selected,active)?L"Edits in Remap and Configuration are saved automatically. Undo restores the settings from when this profile was activated.":
        L"Browsing only. Activate this profile to edit its bindings and curves."):L"The active profile has not been discarded.");
    Layout();
}
inline void EvaluateForeground() {
    HWND foreground=GetForegroundWindow();if(!foreground)return;
    DWORD pid=0;GetWindowThreadProcessId(foreground,&pid);
    if(pid==GetCurrentProcessId())return; // Includes settings, editor, dialogs and tray menu.
    const auto exe=ProcessPath(pid);if(exe.empty())return;
    lastExternalExe=exe;
    const auto target=Resolve(session.catalog,exe,false,session.manual);
    if(!target.empty()&&!Same(target,GlobalProfiles_GetActiveName())) {
        if(session.Activate(target,false))Applied();
        if(IsWindowVisible(page)&&!IsIconic(GetAncestor(page,GA_ROOT)))Update();
    }
}
inline void CALLBACK ForegroundEvent(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) {
    if(page)PostMessageW(page,ForegroundChanged,0,0);
}
inline std::wstring UniqueName(const std::wstring& stem) {
    if(NewName(stem))return stem;
    for(int i=2;i<10000;++i){auto n=stem+L" "+std::to_wstring(i);if(NewName(n))return n;}
    return {};
}
inline void Layout() {
    RECT r{};GetClientRect(page,&r);const int dpi=GetDpiForWindow(page);
    auto s=[&](int n){return MulDiv(n,dpi,96);};
    const int width=r.right,margin=s(12),left=std::clamp(width/4,s(130),s(220)),x=margin+left+s(14);
    const int right=std::max(s(150),width-x-margin),half=(right-s(6))/2;
    const int top=s(session.error.empty()?42:98);
    const int extent=s(510),visible=std::max(1,int(r.bottom)-top);
    SetWindowPos(body,nullptr,x,top,right,visible,SWP_NOZORDER);
    scroll=std::clamp(scroll,0,std::max(0,extent-visible));
    SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS,0,extent-1,(UINT)visible,scroll};SetScrollInfo(page,SB_VERT,&si,TRUE);
    auto move=[&](int id,int xx,int yy,int w,int h,bool fixed=false){SetWindowPos(Control(id),nullptr,fixed?xx:xx-x,fixed?yy:yy-s(42)-scroll,std::max(1,w),h,SWP_NOZORDER);};
    move(Status,margin,s(8),width-margin*2,s(23),true);
    move(Error,margin,s(34),width-margin*2,s(58),true);
    move(List,margin,top,left,std::max(s(20),int(r.bottom)-top-s(46)),true);
    move(New,margin,std::max(s(108),int(r.bottom)-s(38)),left,s(28),true);
    move(NameLabel,x,s(42),right,s(18));move(Name,x,s(64),right,s(26));
    move(Activate,x,s(99),half,s(28));move(Duplicate,x+half+s(6),s(99),half,s(28));
    move(Rename,x,s(133),half,s(28));move(Delete,x+half+s(6),s(133),half,s(28));
    move(GamesLabel,x,s(174),right,s(19));move(Games,x,s(198),right,s(86));
    move(AddExe,x,s(291),half,s(28));move(AddRunning,x+half+s(6),s(291),half,s(28));
    move(RemoveGame,x,s(325),right,s(28));
    move(Automatic,x,s(367),right,s(26));move(Resume,x,s(400),half,s(28));move(Undo,x+half+s(6),s(400),half,s(28));
    move(Details,x,s(441),right,s(103));
    InvalidateRect(page,nullptr,TRUE);
}
inline void BrowseExe() {
    wchar_t path[32768]{};OPENFILENAMEW of{sizeof(of)};of.hwndOwner=page;
    of.lpstrFilter=L"Applications (*.exe)\0*.exe\0\0";of.lpstrFile=path;of.nMaxFile=_countof(path);
    of.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;of.lpstrTitle=L"Choose the game's executable (not its launcher)";
    if(GetOpenFileNameW(&of))session.Assign(path,selected);
}
struct Running {std::wstring path,title;};
inline void AddRunningGame() {
    std::vector<Running> apps;
    EnumWindows([](HWND w,LPARAM raw)->BOOL {
        if(!IsWindowVisible(w)||GetWindow(w,GW_OWNER))return TRUE;
        wchar_t title[256]{};if(!GetWindowTextW(w,title,256))return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(w,&pid);if(pid==GetCurrentProcessId())return TRUE;
        auto path=ProcessPath(pid);if(path.empty())return TRUE;
        auto& list=*reinterpret_cast<std::vector<Running>*>(raw);
        for(const auto& a:list)if(Same(a.path,path))return TRUE;
        if(list.size()<100)list.push_back({std::move(path),title});return TRUE;
    },(LPARAM)&apps);
    HMENU menu=CreatePopupMenu();if(!menu)return;
    for(size_t i=0;i<apps.size();++i) {
        auto label=apps[i].title+L" ("+std::filesystem::path(apps[i].path).filename().wstring()+L")";
        std::replace(label.begin(),label.end(),L'&',L' ');AppendMenuW(menu,MF_STRING,(UINT_PTR)i+1,label.c_str());
    }
    if(apps.empty())AppendMenuW(menu,MF_STRING|MF_DISABLED,0,L"No accessible applications");
    RECT r{};GetWindowRect(Control(AddRunning),&r);
    const auto id=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,r.left,r.bottom,0,page,nullptr);DestroyMenu(menu);
    if(id&&id<=apps.size())session.Assign(apps[id-1].path,selected);
}
inline LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    switch(m) {
    case WM_CREATE: {
        page=h;session.Initialize();selected=GlobalProfiles_GetActiveName();
        session.canActivate=[](const std::wstring& name,std::wstring& error) {
            if(!KeychronOnboard_OwnsOutput())return true;
            const auto path=GlobalProfiles_GetSettingsPath(name);
            // Disabled output is a legitimate desktop profile.
            if(!GetPrivateProfileIntW(L"Main",L"VirtualGamepadsEnabled",1,path.c_str()))return true;
            bool supported=GetPrivateProfileIntW(L"Main",L"VirtualGamepads",1,path.c_str())==1 &&
                GetPrivateProfileIntW(L"Main",L"MouseToStickEnabled",0,path.c_str())==0;
            BindingsSnapshot bindings;
            if(!Profile_PrepareIni(GlobalProfiles_GetBindingsPath(name).c_str(),bindings))return false;
            for(const auto& axis:bindings.axes[0])supported &= k4_onboard::SlotForHid(axis.minusHid)>=0&&k4_onboard::SlotForHid(axis.plusHid)>=0;
            for(auto hid:bindings.triggers[0])supported &= k4_onboard::SlotForHid(hid)>=0;
            for(const auto& button:bindings.buttons[0])for(unsigned hid=1;hid<keycode::kCount;++hid)
                if(button[hid/64]&(uint64_t{1}<<(hid%64)))supported &= k4_onboard::SlotForHid(static_cast<uint16_t>(hid))>=0;
            if(!supported)error=L"This profile requires inputs unavailable in the current onboard mode. The previous profile is still active.";
            return supported;
        };
        WNDCLASSW viewport{};viewport.hInstance=GetModuleHandleW(nullptr);viewport.lpszClassName=L"HallJoyProfileDetails";
        viewport.lpfnWndProc=[](HWND v,UINT msg,WPARAM wp,LPARAM lp)->LRESULT {
            if(msg==WM_COMMAND||msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORBTN||msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORLISTBOX||msg==WM_MOUSEWHEEL)
                return SendMessageW(GetParent(v),msg,wp,lp);
            return DefWindowProcW(v,msg,wp,lp);
        };
        viewport.hbrBackground=UiTheme::Brush_PanelBg();RegisterClassW(&viewport);
        body=CreateWindowW(viewport.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,10,10,h,nullptr,viewport.hInstance,nullptr);
        HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
        auto add=[&](int id,const wchar_t* cls,const wchar_t* text,DWORD style) {
            HWND child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,10,10,(id==List||id==Status||id==New||id==Error)?h:body,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
            SendMessageW(child,WM_SETFONT,(WPARAM)font,FALSE);UiTheme::ApplyToControl(child);return child;
        };
        add(List,L"LISTBOX",L"",WS_TABSTOP|LBS_NOTIFY|WS_VSCROLL|LBS_NOINTEGRALHEIGHT);
        add(Status,L"STATIC",L"",0);add(Error,L"STATIC",L"",0);add(NameLabel,L"STATIC",L"Profile name",0);
        add(Name,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL|WS_BORDER);SendMessageW(Control(Name),EM_SETLIMITTEXT,120,0);
        const std::pair<int,const wchar_t*> buttons[]={{Activate,L"Activate"},{Duplicate,L"Duplicate"},{Rename,L"Rename"},{Delete,L"Delete"},{New,L"+ New profile"},
            {AddExe,L"Choose EXE..."},{AddRunning,L"Running app..."},{RemoveGame,L"Remove selected game"},{Resume,L"Return to automatic"},{Undo,L"Undo edits"}};
        for(const auto& b:buttons)add(b.first,L"BUTTON",b.second,WS_TABSTOP|BS_PUSHBUTTON);
        add(GamesLabel,L"STATIC",L"Assigned games",0);add(Games,L"LISTBOX",L"",WS_TABSTOP|LBS_NOTIFY|WS_VSCROLL|WS_HSCROLL|LBS_NOINTEGRALHEIGHT);
        SendMessageW(Control(Games),LB_SETHORIZONTALEXTENT,1600,0);
        add(Automatic,L"BUTTON",L"Automatically select profiles for games",WS_TABSTOP|BS_AUTOCHECKBOX);
        add(Details,L"STATIC",L"",0);
        Update();Layout();
        hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,ForegroundEvent,0,0,WINEVENT_OUTOFCONTEXT);
        if(!hook){session.error=L"Foreground window tracking is unavailable. Manual activation still works.";Update();}
        PostMessageW(h,ForegroundChanged,0,0);return 0;
    }
    case ForegroundChanged: SetTimer(h,ForegroundTimer,80,nullptr);return 0;
    case WM_TIMER: if(w==ForegroundTimer){KillTimer(h,ForegroundTimer);EvaluateForeground();return 0;}break;
    case Refresh:Update();return 0;
    case WM_SHOWWINDOW:if(w){Update();Layout();}break;
    case WM_SIZE:Layout();return 0;
    case WM_MOUSEWHEEL:scroll-=GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*48;Layout();return 0;
    case WM_VSCROLL: {
        SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(h,SB_VERT,&si);
        switch(LOWORD(w)){case SB_LINEUP:scroll-=24;break;case SB_LINEDOWN:scroll+=24;break;case SB_PAGEUP:scroll-=si.nPage;break;case SB_PAGEDOWN:scroll+=si.nPage;break;case SB_THUMBTRACK:scroll=si.nTrackPos;break;}
        Layout();return 0;
    }
    case WM_COMMAND: {
        const int id=LOWORD(w),code=HIWORD(w);
        if(id==List&&code==LBN_SELCHANGE) {const auto i=SendMessageW(Control(List),LB_GETCURSEL,0,0);if(i>=0&&size_t(i)<names.size()){selected=names[i];Update();}return 0;}
        if(code!=BN_CLICKED)return 0;
        switch(id) {
        case Activate:if(session.Activate(selected,true))Applied();break;
        case Duplicate: {const auto n=UniqueName(selected+L" copy");if(profiles::Duplicate(selected,n))selected=n;else session.error=L"Could not duplicate this profile.";break;}
        case New: {
            HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"From current settings");AppendMenuW(menu,MF_STRING,2,L"Factory defaults (empty bindings)");
            RECT r{};GetWindowRect(Control(New),&r);int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,r.left,r.top,0,h,nullptr);DestroyMenu(menu);
            if(choice){auto n=UniqueName(L"New profile");if(profiles::Duplicate(GlobalProfiles_GetActiveName(),n,choice==2))selected=n;else session.error=L"Could not create a profile.";}break;
        }
        case Rename: {const auto name=Text(Name);if(session.Rename(selected,name)){selected=name;Applied();}break;}
        case Delete:if(MessageBoxW(h,(L"Delete profile '"+selected+L"' and its game associations? A recovery copy will be kept.").c_str(),L"Profiles",MB_YESNO|MB_ICONQUESTION)==IDYES)session.Remove(selected);break;
        case AddExe:BrowseExe();break;
        case AddRunning:AddRunningGame();break;
        case RemoveGame: {const auto i=SendMessageW(Control(Games),LB_GETCURSEL,0,0);if(i>=0&&size_t(i)<gameRows.size()){auto c=session.catalog;c.games.erase(c.games.begin()+gameRows[i]);session.Store(std::move(c));}break;}
        case Automatic: {auto c=session.catalog;c.automatic=SendMessageW(Control(Automatic),BM_GETCHECK,0,0)==BST_CHECKED;if(session.Store(c)){session.manual=false;const auto n=Resolve(c,lastExternalExe,false,false);if(!n.empty()&&session.Activate(n,false))Applied();}break;}
        case Resume:session.manual=false;{const auto n=Resolve(session.catalog,lastExternalExe,false,false);if(!n.empty()&&session.Activate(n,false))Applied();}break;
        case Undo:if(MessageBoxW(h,L"Undo all edits made since this profile was activated?",L"Profiles",MB_YESNO|MB_ICONQUESTION)==IDYES&&session.Undo())Applied();break;
        default:return 0;
        }
        Update();
        if(id==New||id==Duplicate){SetFocus(Control(Name));SendMessageW(Control(Name),EM_SETSEL,0,-1);}
        return 0;
    }
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)w,(HWND)l==Control(Error)?RGB(255,185,100):UiTheme::Color_Text());SetBkColor((HDC)w,UiTheme::Color_PanelBg());return (LRESULT)UiTheme::Brush_PanelBg();
    case WM_ERASEBKGND:{RECT r{};GetClientRect(h,&r);FillRect((HDC)w,&r,UiTheme::Brush_PanelBg());return 1;}
    case WM_DESTROY:if(hook)UnhookWinEvent(hook);hook=nullptr;KillTimer(h,ForegroundTimer);page=nullptr;body=nullptr;session={};visualPending=false;lastExternalExe.clear();scroll=0;return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
inline HWND Create(HWND parent,HINSTANCE instance) {
    WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"HallJoyProfilesPage";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);
    return CreateWindowW(wc.lpszClassName,L"",WS_CHILD|WS_CLIPCHILDREN|WS_VSCROLL,0,0,100,100,parent,nullptr,instance,nullptr);
}
}
