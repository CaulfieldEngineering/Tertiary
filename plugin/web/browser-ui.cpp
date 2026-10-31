// Production web-only UI hook. A menu's screen coordinates are stale after rotation.
#include <juce_gui_basics/juce_gui_basics.h>
#include <emscripten.h>

extern "C" EMSCRIPTEN_KEEPALIVE void wonderlab_viewportChanged()
{
    juce::PopupMenu::dismissAllActiveMenus();
}

// Keep popup hit testing and drawing in the same JUCE coordinates when a phone
// zoom crops the editor. Moving only the HTML canvas would leave taps displaced.
extern "C" EMSCRIPTEN_KEEPALIVE void wonderlab_fitPopup (uintptr_t peerAddress,
                                                       int x, int y, int width, int height)
{
    const juce::Rectangle<int> visibleArea (x, y, width, height);
    for (int i = 0; i < juce::ComponentPeer::getNumPeers(); ++i)
        if (auto* peer = juce::ComponentPeer::getPeer(i);
            reinterpret_cast<uintptr_t>(peer) == peerAddress)
        {
            auto& component = peer->getComponent();
            // Peer bounds include JUCE's popup frame; the component's origin can
            // be inset by a pixel, which becomes visible when zoomed on a phone.
            const auto bounds = peer->getBounds();
            const auto fitted = bounds.constrainedWithin(visibleArea);
            component.setBounds(component.getX() + fitted.getX() - bounds.getX(),
                                component.getY() + fitted.getY() - bounds.getY(),
                                component.getWidth() + fitted.getWidth() - bounds.getWidth(),
                                component.getHeight() + fitted.getHeight() - bounds.getHeight());
            return;
        }
}
