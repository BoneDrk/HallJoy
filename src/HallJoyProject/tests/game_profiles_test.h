#pragma once
#include "../HallJoy/game_profiles.h"
namespace halljoy::profiles::test {
inline bool Run() {
    const auto original=GlobalProfiles_GetActiveName();
    if(!GlobalProfiles_Save(original))return false;
    std::function<void()> restore;if(!GlobalProfiles_Prepare(original,restore))return false;
    bool ok=true;
    Session s;s.Initialize();
    const auto beforeLow=Settings_GetInputDeadzoneLow();
    ok &= Duplicate(original,L"Game Test Copy");
    ok &= !Duplicate(original,L"game test copy");
    ok &= !Duplicate(original,L"../unsafe");
    ok &= Duplicate(original,L"Game Test Factory",true);
    ok &= GlobalProfiles_GetActiveName()==original && Settings_GetInputDeadzoneLow()==beforeLow;
    ok &= s.Assign(L"C:\\Games\\Example\\game.exe",L"Game Test Copy");
    ok &= !s.Assign(L"c:\\games\\example\\GAME.exe",L"Game Test Factory");
    auto c=s.catalog;c.automatic=true;ok &= s.Store(c);
    ok &= Resolve(c,L"C:\\Games\\Example\\game.exe",false,false)==L"Game Test Copy";
    ok &= Resolve(c,L"C:\\Windows\\explorer.exe",false,false)==L"Default";
    ok &= Resolve(c,L"C:\\Windows\\explorer.exe",true,false).empty();
    ok &= Resolve(c,L"C:\\Windows\\explorer.exe",false,true).empty();
    ok &= Resolve(c,L"",false,false).empty();
    Catalog loaded;ok &= ReadCatalog(CatalogPath().c_str(),loaded)&&loaded.automatic&&loaded.games.size()==1;
    IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::Replace);
    auto rejected=c;rejected.automatic=false;ok &= !s.Store(rejected);
    IniUtil_TestSetFailureStage(HallJoyPersistence::SaveStage::None);
    ok &= s.catalog.automatic&&ReadCatalog(CatalogPath().c_str(),loaded)&&loaded.automatic;
    const auto generation=profile_runtime::revision.load();
    ok &= s.Activate(L"Game Test Copy",true)&&s.manual&&profile_runtime::revision.load()>generation;
    Settings_SetInputDeadzoneLow(.37f);
    ok &= GlobalProfiles_Save(GlobalProfiles_GetActiveName()); // Autosave must not destroy the editing checkpoint.
    ok &= s.Undo()&&std::abs(Settings_GetInputDeadzoneLow()-beforeLow)<.001f;
    // A backend compatibility rejection must happen before any profile mutation.
    s.canActivate=[](const std::wstring&,std::wstring& error){error=L"test rejection";return false;};
    const auto rejectedActive=GlobalProfiles_GetActiveName();
    ok &= !s.Activate(L"Game Test Factory",false)&&GlobalProfiles_GetActiveName()==rejectedActive;
    s.canActivate={};
    const auto active=GlobalProfiles_GetActiveName();
    ok &= !s.Activate(L"Missing game profile",false)&&GlobalProfiles_GetActiveName()==active;
    ok &= !s.Remove(active);
    ok &= s.Rename(L"Game Test Copy",L"Game Test Renamed");
    ok &= GlobalProfiles_GetActiveName()==L"Game Test Renamed"&&s.catalog.games[0].profile==L"Game Test Renamed";
    ok &= !Exists(L"Game Test Copy");
    ok &= s.Activate(L"Game Test Factory",false);
    ok &= std::abs(Settings_GetInputDeadzoneLow()-.08f)<.001f && Bindings_GetAxis(Axis::LX).plusHid==0;
    ok &= s.Remove(L"Game Test Renamed")&&!Exists(L"Game Test Renamed")&&s.catalog.games.empty();
    ok &= !s.Remove(L"Default")&&!s.Rename(L"Default",L"Bad rename");
    ok &= s.Activate(original,false);
    {profile_runtime::CommitLease lease;if(lease)restore();else ok=false;}
    ok &= s.Remove(L"Game Test Factory");
    ok &= s.Store({});
    const auto path=CatalogPath();
    ok &= WritePrivateProfileStringW(L"Profiles",L"Version",L"999",path.c_str())!=FALSE;
    Session damaged;damaged.Initialize();
    ok &= damaged.readOnly&&!damaged.error.empty()&&!damaged.Store({});
    wchar_t version[16]{};GetPrivateProfileStringW(L"Profiles",L"Version",L"",version,16,path.c_str());
    ok &= wcscmp(version,L"999")==0; // Corrupt/unknown versions are preserved, never silently replaced.
    ok &= SaveCatalog({});
    return ok;
}
}
