#pragma once
#include <string_view>
namespace halljoy::uap_discovery {
constexpr bool SupportedVendor(unsigned vid) noexcept {
 // Keep in sync with Soup AnalogueKeyboard::checkDeviceName. Unknown vendors
 // have no UAP input route and must not be opened for read/write discovery.
 return vid==0x31e3 || vid==0x03eb || vid==0x1532 || vid==0x352d ||
        vid==0x3434 || vid==0x362d || vid==0x19f5 || vid==0x373b;
}
constexpr wchar_t Lower(wchar_t c) noexcept {return c>=L'A' && c<=L'Z'?c+(L'a'-L'A'):c;}
constexpr int Hex(wchar_t c) noexcept {c=Lower(c);return c>=L'0' && c<=L'9'?c-L'0':c>=L'a' && c<=L'f'?c-L'a'+10:-1;}
inline bool MayUsePath(std::wstring_view p) noexcept {
 for(std::size_t i=0;i+8<=p.size();++i) {
  if(Lower(p[i])!=L'v' || Lower(p[i+1])!=L'i' || Lower(p[i+2])!=L'd' || p[i+3]!=L'_')continue;
  unsigned vid=0;
  for(unsigned j=0;j<4;++j){const int h=Hex(p[i+4+j]);if(h<0)return true;vid=(vid<<4)|unsigned(h);}
  if(i+8<p.size() && p[i+8]!=L'&' && p[i+8]!=L'#')return true;
  return SupportedVendor(vid);
 }
 return true; // Nonstandard/Bluetooth identities retain historical behavior.
}
}
