// Production web-only UI hook. A menu's screen coordinates are stale after rotation.
#include <juce_gui_basics/juce_gui_basics.h>
#include <emscripten.h>

extern "C" EMSCRIPTEN_KEEPALIVE void wonderlab_viewportChanged()
{
    juce::PopupMenu::dismissAllActiveMenus();
}
