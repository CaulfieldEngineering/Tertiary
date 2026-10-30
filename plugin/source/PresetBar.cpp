/*
  ==============================================================================

    PresetBar.cpp
    Created: 27 Oct 2026
    Author:  Claude (for Joe), Tertiary 1.1

    [Claude 2026-10-27] New file. See PresetBar.h.

  ==============================================================================
*/

#include "PresetBar.h"
#include "MyLookAndFeel.h"
#include "UtilityFunctions.h"   // [Claude 2026-10-29] PixelGrid

namespace
{
    // The same quiet greys as the About window's text fields
    const juce::Colour background = juce::Colours::black.withAlpha(0.35f);
    const juce::Colour outline    = juce::Colours::grey.withAlpha(0.6f);
    const juce::Colour text       = juce::Colours::lightgrey;

    enum MenuIds { menuSave = 10000, menuSaveAs, menuRename, menuDelete, menuOpenFolder, menuFirstPreset = 1 };
}

PresetBar::PresetBar(audio_plugin::AudioPluginAudioProcessor& p)
    : audioProcessor(p), presets(p.presets)
{
    for (auto* b : { &mButtonPrevious, &mButtonNext })
        addAndMakeVisible(b);
    mButtonPrevious.setTooltip("Previous preset");
    mButtonNext.setTooltip("Next preset");
    mButtonPrevious.onClick = [this] { presets.loadPrevious(); changed(); };
    mButtonNext.onClick     = [this] { presets.loadNext(); changed(); };

    startTimerHz(10);   // follows changes from the host (program change, automation, session load)
}

PresetBar::~PresetBar()
{
}

void PresetBar::ArrowKey::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    // a small raised grey key with a lit top edge, pressed darker, and a fine chevron
    const PixelGrid grid(g, this);   // [Claude 2026-10-29] edges and highlight on whole screen pixels
    auto r = grid.strokeRect(getLocalBounds().toFloat());
    const auto top = down ? juce::Colour(0xff26282e) : juce::Colour(0xff34363e);
    const auto bot = down ? juce::Colour(0xff1f2126) : juce::Colour(0xff25272d);
    g.setGradientFill(juce::ColourGradient(top, 0.f, r.getY(), bot, 0.f, r.getBottom(), false));
    g.fillRoundedRectangle(r, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(highlighted ? 0.16f : 0.1f));
    fillHLine(g, grid, r.getY() + grid.rulePx() / 2.f, r.getX() + 1.5f, r.getRight() - 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(r, 1.5f, grid.rulePx());

    const float cx = r.getCentreX(), cy = r.getCentreY(), w = 2.5f, h = 4.f;
    juce::Path chevron;
    chevron.startNewSubPath(cx + (isLeft ? w : -w) / 2.f, cy - h);
    chevron.lineTo(cx - (isLeft ? w : -w) / 2.f, cy);
    chevron.lineTo(cx + (isLeft ? w : -w) / 2.f, cy + h);
    g.setColour(juce::Colour(highlighted ? 0xffd9dade : 0xffa9abb3));
    g.strokePath(chevron, juce::PathStrokeType(1.3f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
}

void PresetBar::resized()
{
    auto bounds = getLocalBounds();
    mButtonPrevious.setBounds(bounds.removeFromLeft(getHeight() - 2).reduced(0, 1));
    mButtonNext.setBounds(bounds.removeFromRight(getHeight() - 2).reduced(0, 1));
    nameArea = bounds.reduced(4, 0);
}

void PresetBar::paint(juce::Graphics& g)
{
    // [Claude 2026-10-29] The name in a recessed well, in the header's text size (Joe: "let's restyle it")
    const PixelGrid grid(g, this);   // [Claude 2026-10-29] the well's edges and the name on whole screen pixels
    auto area = grid.strokeRect(nameArea.toFloat());
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0b0c0f), 0.f, area.getY(), juce::Colour(0xff15161a), 0.f, area.getBottom(), false));
    g.fillRoundedRectangle(area, 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    fillHLine(g, grid, area.getY(), area.getX() + 1.5f, area.getRight() - 1.5f, grid.thick(1.5f));
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    fillHLine(g, grid, area.getBottom() - grid.rulePx(), area.getX() + 1.5f, area.getRight() - 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(area, 1.5f, grid.rulePx());

    g.setColour(juce::Colour(0xffd6d7dc));
    g.setFont(juce::Font(12.5f));
    auto label = shownName + (shownModified ? " *" : "");
    drawTextOnPixels(g, grid, label, nameArea.reduced(6, 0).withTrimmedRight(12).toFloat(), juce::Justification::centredLeft);

    // A small down-pointing triangle: this opens a menu
    auto r = nameArea.withTrimmedLeft(nameArea.getWidth() - 14).toFloat().withSizeKeepingCentre(7.f, 4.f).translated(-4.f, 0.f);
    juce::Path arrow;
    arrow.addTriangle(r.getX(), r.getY(), r.getRight(), r.getY(), r.getCentreX(), r.getBottom());
    g.setColour(juce::Colour(0xff6c6f78));
    g.fillPath(arrow);
}

void PresetBar::timerCallback()
{
    auto name = presets.getCurrentName();
    auto modified = presets.isModified();
    if (name != shownName || modified != shownModified)
    {
        shownName = name;
        shownModified = modified;
        repaint();
    }
}

void PresetBar::mouseUp(const juce::MouseEvent& event)
{
    if (nameArea.contains(event.getPosition()))
        showMenu();
}

void PresetBar::changed()
{
    audioProcessor.presetListChanged();
    timerCallback();
}

//==============================================================================
void PresetBar::showMenu()
{
    presets.refreshUserPresets();   // presets saved by another copy of Tertiary show up too

    juce::PopupMenu menu;
    menu.setLookAndFeel(&HouseMenuLookAndFeel::get());   // [Claude 2026-10-29]
    const int current = presets.getCurrentIndex();
    const int numFactory = (int) PresetManager::getFactoryPresets().size();

    menu.addSectionHeader("Factory presets");
    for (int i = 0; i < numFactory; ++i)
        menu.addItem(menuFirstPreset + i, presets.getPresetName(i), true, i == current);

    menu.addSectionHeader("Your presets");
    if (presets.getNumPresets() == numFactory)
        menu.addItem(-1, "(none yet: use Save As)", false);
    for (int i = numFactory; i < presets.getNumPresets(); ++i)
        menu.addItem(menuFirstPreset + i, presets.getPresetName(i), true, i == current);

    const bool userCurrent = presets.isUserPreset(current);
    menu.addSeparator();
    menu.addItem(menuSave, "Save", userCurrent);
    menu.addItem(menuSaveAs, "Save As...");
    menu.addItem(menuRename, "Rename...", userCurrent);
    menu.addItem(menuDelete, "Delete", userCurrent);
   #if ! JUCE_EMSCRIPTEN
    menu.addSeparator();
    menu.addItem(menuOpenFolder, "Open Presets Folder");
   #endif

    juce::Component::SafePointer<PresetBar> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            auto& self = *safeThis;
            if (result == menuSave)            { self.presets.saveCurrentAsUser(self.presets.getCurrentName()); self.changed(); }
            else if (result == menuSaveAs)     self.saveAs();
            else if (result == menuRename)     self.renameCurrent();
            else if (result == menuDelete)     self.deleteCurrent();
            else if (result == menuOpenFolder)
            {
                self.presets.getUserFolder().createDirectory();
                TestHooks::openFolder(self.presets.getUserFolder());   // [Claude 2026-10-29] UI tests: recorded, not opened
            }
            else
            {
                self.presets.loadPreset(result - menuFirstPreset);
                self.changed();
            }
        });
}

/* A small window with a text field, OK and Cancel */
void PresetBar::askForName(const juce::String& title, const juce::String& initial, std::function<void(juce::String)> onOk)
{
    auto* window = new juce::AlertWindow(title, "Name:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", initial);
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safeThis(this);
    window->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, window, onOk](int result)
    {
        auto name = window->getTextEditorContents("name");
        if (safeThis != nullptr && result == 1)
            onOk(name);
    }), true);
}

void PresetBar::saveAs()
{
    auto initial = presets.isUserPreset(presets.getCurrentIndex()) ? presets.getCurrentName() : juce::String();
    juce::Component::SafePointer<PresetBar> safeThis(this);

    askForName("Save preset", initial, [safeThis](juce::String name)
    {
        if (safeThis == nullptr)
            return;
        auto& self = *safeThis;
        auto save = [safeThis, name]
        {
            if (safeThis == nullptr)
                return;
            if (! safeThis->presets.saveCurrentAsUser(name))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save preset",
                    "Couldn't save \"" + name + "\". Is it a factory preset's name, or empty?", "OK", safeThis.getComponent());
            safeThis->changed();
        };

        if (self.presets.userPresetExists(name))
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Save preset",
                "\"" + PresetManager::cleanName(name) + "\" already exists. Replace it?", "Replace", "Cancel", &self,
                juce::ModalCallbackFunction::create([save](int r) { if (r == 1) save(); }));
        else
            save();
    });
}

void PresetBar::renameCurrent()
{
    const int index = presets.getCurrentIndex();
    juce::Component::SafePointer<PresetBar> safeThis(this);
    askForName("Rename preset", presets.getCurrentName(), [safeThis, index](juce::String name)
    {
        if (safeThis == nullptr)
            return;
        if (! safeThis->presets.renameUser(index, name))
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Rename preset",
                "Couldn't rename to \"" + name + "\". Is the name taken?", "OK", safeThis.getComponent());
        safeThis->changed();
    });
}

void PresetBar::deleteCurrent()
{
    const int index = presets.getCurrentIndex();
    juce::Component::SafePointer<PresetBar> safeThis(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Delete preset",
        "Delete \"" + presets.getCurrentName() + "\"? It goes to the Recycle Bin / Trash. The current settings stay.",
        "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create([safeThis, index](int r)
        {
            if (safeThis != nullptr && r == 1)
            {
                safeThis->presets.deleteUser(index);
                safeThis->changed();
            }
        }));
}
