#pragma once
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include "global_profiles.h"
#include "app_paths.h"
#include "file_name_policy.h"
#include "ini_util.h"
#include "ini_write_batch.h"
#include "bounded_ini.h"
#include "settings_ini.h"
#include "profile_ini.h"
#include "profile_runtime_gate.h"

// UI-thread owner. Gameplay never reads files or enumerates processes.
namespace halljoy::profiles {
struct Association { std::wstring exe, profile; };
struct Catalog { bool automatic = false; std::vector<Association> games; };
inline bool Same(const std::wstring& a,const std::wstring& b) {return _wcsicmp(a.c_str(),b.c_str())==0;}
inline std::wstring CatalogPath() { return (std::filesystem::path(AppPaths_DataRoot())/L"GameProfiles.ini").wstring(); }
inline bool ReadCatalog(const wchar_t* path,Catalog& result) {
    ini::ReadFile file(path); if(!file)return false;
    std::wstring version,enabled,count;
    if(!ini::Read(path,L"Profiles",L"Version",version)||version!=L"1" ||
       !ini::Read(path,L"Profiles",L"Automatic",enabled)||
       !ini::Read(path,L"Profiles",L"Count",count))return false;
    uint32_t e=0,n=0; if(!ini::Unsigned(enabled,1,e)||!ini::Unsigned(count,512,n))return false;
    Catalog next;next.automatic=e!=0;
    for(uint32_t i=0;i<n;++i) {
        Association a;const auto section=L"Game"+std::to_wstring(i);
        if(!ini::Read(path,section.c_str(),L"Exe",a.exe,32768)||a.exe.empty()||
           !ini::Read(path,section.c_str(),L"Profile",a.profile,256)||a.profile.empty()||
           GlobalProfiles_SanitizeName(a.profile)!=a.profile)return false;
        for(const auto& old:next.games)if(Same(old.exe,a.exe))return false;
        next.games.push_back(std::move(a));
    }
    result=std::move(next);return true;
}
inline bool SaveCatalog(const Catalog& catalog) {
    if(IniUtil_IsSessionReadOnly()||catalog.games.size()>512)return false;
    const auto writer=[](const wchar_t* p,void* raw,DWORD* error) {
        const auto& c=*static_cast<const Catalog*>(raw);ini::WriteBatch b(p);
        bool ok=b.Put(L"Profiles",L"Version",L"1",p)&&b.Put(L"Profiles",L"Automatic",c.automatic?L"1":L"0",p)&&
            b.Put(L"Profiles",L"Count",std::to_wstring(c.games.size()).c_str(),p);
        for(size_t i=0;i<c.games.size();++i) {
            const auto s=L"Game"+std::to_wstring(i);
            ok=ok&&b.Put(s.c_str(),L"Exe",c.games[i].exe.c_str(),p)&&b.Put(s.c_str(),L"Profile",c.games[i].profile.c_str(),p);
        }
        return ok&&b.Finish(error);
    };
    const auto validate=[](const wchar_t* p,void* raw,DWORD*) {
        Catalog c;const auto& expected=*static_cast<const Catalog*>(raw);
        if(!ReadCatalog(p,c)||c.automatic!=expected.automatic||c.games.size()!=expected.games.size())return false;
        for(size_t i=0;i<c.games.size();++i)if(c.games[i].exe!=expected.games[i].exe||c.games[i].profile!=expected.games[i].profile)return false;
        return true;
    };
    return IniUtil_SaveAtomic(CatalogPath().c_str(),writer,validate,const_cast<Catalog*>(&catalog)).Succeeded();
}
inline std::wstring Resolve(const Catalog& catalog,const std::wstring& exe,bool ownWindow,bool manual) {
    if(!catalog.automatic||manual||ownWindow||exe.empty())return {}; // Unknown identity preserves the last safe choice.
    for(const auto& a:catalog.games)if(Same(a.exe,exe))return a.profile;
    return L"Default";
}
inline bool Exists(const std::wstring& name) {
    std::vector<std::wstring> names;GlobalProfiles_List(names);
    for(const auto& n:names)if(FileNamePolicy_Equivalent(n,name))return true;
    return false;
}
inline bool NewName(const std::wstring& name) {
    return !name.empty()&&name==GlobalProfiles_SanitizeName(name)&&!Exists(name)&&
        GetFileAttributesW(GlobalProfiles_GetBindingsPath(name).c_str())==INVALID_FILE_ATTRIBUTES;
}
// Copy without ever applying the browsed profile. Legacy pairs remain readable.
inline bool Duplicate(const std::wstring& source,const std::wstring& name,bool factory=false) {
    if(IniUtil_IsSessionReadOnly()||!NewName(name))return false;
    if(!factory && Same(source,GlobalProfiles_GetActiveName())&&!GlobalProfiles_Save(source))return false;
    if(!factory) {std::function<void()> proof;if(!GlobalProfiles_Prepare(source,proof))return false;}
    const auto settings=GlobalProfiles_GetSettingsPath(name);
    const auto bindings=GlobalProfiles_GetBindingsPath(name);
    const auto stage=settings+L".new";
    if(GetFileAttributesW(stage.c_str())!=INVALID_FILE_ATTRIBUTES)return false;
    bool copiedBindings=false,ok=false;
    if(factory) {
        const char seed[]="[Input]\r\nDeadzoneLow=80\r\n[Pad1_Axes]\r\nLX_Plus=0\r\n";
        HANDLE f=CreateFileW(stage.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f==INVALID_HANDLE_VALUE)return false;
        DWORD written=0;ok=WriteFile(f,seed,sizeof(seed)-1,&written,nullptr)&&written==sizeof(seed)-1&&FlushFileBuffers(f);CloseHandle(f);
        if(ok) {ok=CopyFileW(stage.c_str(),bindings.c_str(),TRUE)!=FALSE;copiedBindings=ok;}
    } else {
        ok=CopyFileW(GlobalProfiles_GetSettingsPath(source).c_str(),stage.c_str(),TRUE)!=FALSE;
        if(ok&&!ini::HasBundle(stage.c_str())) {ok=CopyFileW(GlobalProfiles_GetBindingsPath(source).c_str(),bindings.c_str(),TRUE)!=FALSE;copiedBindings=ok;}
    }
    std::function<void()> prepared;BindingsSnapshot bs;
    ok=ok&&SettingsIni_PrepareProfile(stage.c_str(),prepared)&&Profile_PrepareIni((ini::HasBundle(stage.c_str())?stage:bindings).c_str(),bs);
    if(ok)ok=MoveFileExW(stage.c_str(),settings.c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok) {DeleteFileW(stage.c_str());if(copiedBindings)DeleteFileW(bindings.c_str());}
    return ok;
}
inline bool Archive(const std::wstring& name) {
    if(IniUtil_IsSessionReadOnly()||GlobalProfiles_IsDefault(name))return false;
    auto dir=std::filesystem::path(AppPaths_DataRoot())/L".internal"/L"DeletedProfiles"/
        (name+L"-"+std::to_wstring(GetTickCount64()));
    std::error_code error;std::filesystem::create_directories(dir,error);if(error)return false;
    const auto s=GlobalProfiles_GetSettingsPath(name),b=GlobalProfiles_GetBindingsPath(name);
    if(!CopyFileW(s.c_str(),(dir/L"settings.ini").c_str(),TRUE))return false;
    return Same(s,b)||CopyFileW(b.c_str(),(dir/L"bindings.ini").c_str(),TRUE)!=FALSE;
}
struct Session {
    Catalog catalog; bool manual=false,readOnly=false;std::wstring error;
    std::function<void()> checkpoint;
    std::function<bool(const std::wstring&,std::wstring&)> canActivate;
    void Initialize() {
        const auto p=CatalogPath();
        if(GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES&&!ReadCatalog(p.c_str(),catalog)) {
            readOnly=true;error=L"Game associations could not be read. The original file has been preserved.";
        }
        GlobalProfiles_Prepare(GlobalProfiles_GetActiveName(),checkpoint);
    }
    bool Store(Catalog next) {
        if(readOnly||!SaveCatalog(next)){error=L"Could not save game associations. Check access to the HallJoy data folder.";return false;}
        catalog=std::move(next);error.clear();return true;
    }
    bool Assign(const std::wstring& exe,const std::wstring& name) {
        for(const auto& a:catalog.games)if(Same(a.exe,exe)) {
            if(Same(a.profile,name))return true;
            error=L"This executable is already assigned to "+a.profile+L". Remove that association first.";return false;
        }
        auto next=catalog;next.games.push_back({exe,name});return Store(std::move(next));
    }
    bool Activate(const std::wstring& name,bool byUser) {
        if(Same(name,GlobalProfiles_GetActiveName())){if(byUser)manual=true;error.clear();return true;}
        std::function<void()> nextCheckpoint;
        if(!GlobalProfiles_Prepare(name,nextCheckpoint)) {error=L"Could not read profile '"+name+L"'. The previous profile is still active.";return false;}
        if(canActivate&&!canActivate(name,error))return false;
        if(!GlobalProfiles_Switch(name)) {error=L"Could not save or apply profile '"+name+L"'. The previous profile is still active.";return false;}
        checkpoint=std::move(nextCheckpoint);if(byUser)manual=true;error.clear();return true;
    }
    bool Undo() {
        if(!checkpoint){error=L"No editing checkpoint is available.";return false;}
        {profile_runtime::CommitLease lease;if(!lease)return false;checkpoint();profile_runtime::Changed();}
        GlobalProfiles_SetDirty(true);error.clear();return true;
    }
    bool Remove(const std::wstring& name) {
        if(GlobalProfiles_IsDefault(name)||Same(name,GlobalProfiles_GetActiveName())) {
            error=L"Activate another profile before deleting this one.";return false;
        }
        if(!Archive(name)){error=L"Could not back up this profile; it has not been deleted.";return false;}
        auto previous=catalog,next=catalog;
        next.games.erase(std::remove_if(next.games.begin(),next.games.end(),[&](const auto& a){return Same(a.profile,name);}),next.games.end());
        if(!Store(next))return false;
        if(!GlobalProfiles_Delete(name)){Store(previous);error=L"Could not delete the profile. A recovery copy is retained.";return false;}
        return true;
    }
    bool Rename(const std::wstring& old,const std::wstring& name) {
        if(GlobalProfiles_IsDefault(old)||!NewName(name)){error=L"Choose a unique valid name. Default cannot be renamed.";return false;}
        if(!Duplicate(old,name)) {error=L"Could not create the renamed profile.";return false;}
        auto previous=catalog,next=catalog;for(auto& a:next.games)if(Same(a.profile,old))a.profile=name;
        if(!Store(next)){GlobalProfiles_Delete(name);return false;}
        if(Same(old,GlobalProfiles_GetActiveName())&&!Activate(name,false)) {Store(previous);GlobalProfiles_Delete(name);return false;}
        if(!Archive(old)||!GlobalProfiles_Delete(old)) {error=L"The new name is ready, but the old copy could not be removed.";return false;}
        return true;
    }
};
}
