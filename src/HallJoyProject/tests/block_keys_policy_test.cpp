#include "block_keys_policy.h"
#include <cassert>
int main() {
    using namespace halljoy::block_keys;
    // The toggle shortcut itself is covered by input_shortcuts_test. Here: a
    // toggle committed before the next W down; an already passed W still gets
    // its up when blocking changes.
    PressRoutes gameplay;
    bool blocking = true;
    assert(gameplay.Filter(26,true,blocking));
    assert(gameplay.Filter(26,false,blocking));
    assert(!gameplay.HasHeld());
    assert(!gameplay.Filter(26,true,false));
    assert(!gameplay.Filter(26,false,true));
    for (unsigned key = 0; key < 256; ++key)
        assert(IsAltOrTab(key) == (key == 43 || key == 226 || key == 230));
    // Legacy virtual-key chord validation (settings migration only).
    assert(ValidShortcut(0));
    assert(ValidShortcut((6u << 8) | 0x77));
    assert(!ValidShortcut((16u << 8) | 0x77));
    assert(!ValidShortcut(16)); assert(!ValidShortcut(162));
    // Full policy truth table, then physical press ownership through changes.
    for(unsigned bits=0;bits<64;++bits) {
        const bool paused=bits&1,enabled=bits&2,own=bits&4;
        const bool rescue=bits&8,reserved=bits&16,bound=bits&32;
        const bool expected=bits==34;
        assert(ShouldBlock(paused,enabled,own,rescue,reserved,bound)==expected);
    }
    PressRoutes routes;
    assert(!routes.Filter(4, false, true));
    assert(!routes.Filter(0, true, true));
    assert(!routes.Filter(9999, true, true));
    assert(!routes.HasHeld());
    assert(routes.Filter(4, true, true));
    assert(routes.Filter(4, true, false));
    assert(routes.Filter(4, false, false));
    assert(!routes.HasHeld());
    assert(!routes.Filter(4, true, false));
    assert(!routes.Filter(4, true, true));
    assert(!routes.Filter(4, false, true));
    assert(!routes.HasHeld());
    routes.SeedPassed(4); routes.SeedPassed(4);
    assert(!routes.Filter(4, true, true));
    assert(routes.Filter(5, true, true));
    assert(!routes.Filter(4, false, true));
    assert(routes.HasHeld());
    assert(routes.Filter(5, false, false));
    assert(!routes.HasHeld());
    routes.SeedPassed(4); routes.Reset();
    assert(!routes.HasHeld());
}
