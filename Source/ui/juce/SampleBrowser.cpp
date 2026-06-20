#include "SampleBrowser.h"

SampleBrowser::SampleBrowser (const juce::File& startDir)
{
    setSize (560, 460);
    setWantsKeyboardFocus (true);

    pathLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    pathLabel_.setFont (juce::FontOptions (13.0f));
    pathLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (pathLabel_);

    hintLabel_.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    hintLabel_.setFont (juce::FontOptions (11.0f));
    hintLabel_.setJustificationType (juce::Justification::centredLeft);
    hintLabel_.setText (juce::CharPointer_UTF8 ("\xe2\x86\x91\xe2\x86\x93 move   \xe2\x86\x92/Enter open folder   "
                                                "Space/Enter mark file   \xe2\x86\x90 up   then LOAD MARKED"),
                        juce::dontSendNotification);
    addAndMakeVisible (hintLabel_);

    list_.setModel (this);
    list_.setRowHeight (24);
    list_.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1b1b1f));
    list_.setColour (juce::ListBox::outlineColourId, juce::Colours::white.withAlpha (0.15f));
    list_.setOutlineThickness (1);
    list_.onUnhandledKey = [this] (const juce::KeyPress& k) { return keyPressed (k); };
    addAndMakeVisible (list_);

    addAndMakeVisible (upButton_);
    upButton_.onClick = [this] { goUp(); };

    addAndMakeVisible (loadButton_);
    loadButton_.onClick = [this] { confirmLoad(); };

    addAndMakeVisible (cancelButton_);
    cancelButton_.onClick = [this] { if (onCancel) onCancel(); };

    juce::File start = startDir;
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (! start.isDirectory())
        start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    setDirectory (start);
}

void SampleBrowser::setDirectory (const juce::File& dir)
{
    if (! dir.isDirectory())
        return;

    dir_ = dir;
    entries_.clear();

    if (dir_.getParentDirectory() != dir_)
        entries_.push_back ({ dir_.getParentDirectory(), true, true });

    juce::Array<juce::File> subDirs;
    dir_.findChildFiles (subDirs, juce::File::findDirectories | juce::File::ignoreHiddenFiles, false);
    subDirs.sort();
    for (const auto& d : subDirs)
        entries_.push_back ({ d, true, false });

    juce::Array<juce::File> files;
    dir_.findChildFiles (files, juce::File::findFiles | juce::File::ignoreHiddenFiles, false, kAudioExts);
    files.sort();
    for (const auto& f : files)
        entries_.push_back ({ f, false, false });

    pathLabel_.setText (dir_.getFullPathName(), juce::dontSendNotification);
    list_.updateContent();
    list_.selectRow (0);
    list_.grabKeyboardFocus();
}

int SampleBrowser::getNumRows() { return static_cast<int> (entries_.size()); }

void SampleBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= static_cast<int> (entries_.size()))
        return;

    const auto& e = entries_[static_cast<size_t> (row)];
    const bool marked = ! e.isDir && marked_.count (e.file.getFullPathName()) > 0;

    if (selected)
    {
        g.setColour (juce::Colour (0xff3a5f8a));
        g.fillRect (0, 0, w, h);
    }
    else if (marked)
    {
        g.setColour (juce::Colour (0xff2a3a2a));
        g.fillRect (0, 0, w, h);
    }

    auto area = juce::Rectangle<int> (0, 0, w, h).reduced (8, 0);

    // Mark indicator for files.
    g.setColour (marked ? juce::Colours::limegreen : juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::FontOptions (13.0f));
    juce::String tag;
    if (e.isParent)   tag = juce::CharPointer_UTF8 ("\xe2\x86\xb0 ..");   // ↰ ..
    else if (e.isDir) tag = juce::CharPointer_UTF8 ("\xf0\x9f\x93\x81");   // folder glyph
    else              tag = marked ? "[x]" : "[ ]";
    g.drawText (tag, area.removeFromLeft (28), juce::Justification::centredLeft);

    g.setColour (e.isDir ? juce::Colours::white.withAlpha (0.92f)
                         : (marked ? juce::Colours::limegreen : juce::Colours::white.withAlpha (0.82f)));
    const juce::String name = e.isParent ? juce::String ("(parent folder)")
                                         : e.file.getFileName() + (e.isDir ? "/" : "");
    g.drawText (name, area, juce::Justification::centredLeft, true);
}

void SampleBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    activateRow (row);
}

void SampleBrowser::activateRow (int row)
{
    if (row < 0 || row >= static_cast<int> (entries_.size()))
        return;
    const auto& e = entries_[static_cast<size_t> (row)];
    if (e.isParent || e.isDir)
        setDirectory (e.file);
    else
        toggleMarkRow (row);
}

void SampleBrowser::toggleMarkRow (int row)
{
    if (row < 0 || row >= static_cast<int> (entries_.size()))
        return;
    const auto& e = entries_[static_cast<size_t> (row)];
    if (e.isDir)
        return;

    const juce::String path = e.file.getFullPathName();
    if (marked_.count (path) > 0)
        marked_.erase (path);
    else
        marked_.insert (path);

    loadButton_.setButtonText ("LOAD MARKED (" + juce::String (static_cast<int> (marked_.size())) + ")");
    list_.repaintRow (row);
}

void SampleBrowser::goUp()
{
    const juce::File parent = dir_.getParentDirectory();
    if (parent != dir_)
        setDirectory (parent);
}

void SampleBrowser::confirmLoad()
{
    juce::Array<juce::File> files;
    for (const auto& path : marked_)
        files.add (juce::File (path));
    if (onLoad)
        onLoad (files);
}

bool SampleBrowser::keyPressed (const juce::KeyPress& key)
{
    const int row = list_.getSelectedRow();

    if (key == juce::KeyPress::returnKey)
    {
        activateRow (row);
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        toggleMarkRow (row);
        return true;
    }
    if (key == juce::KeyPress::rightKey)
    {
        if (row >= 0 && row < static_cast<int> (entries_.size())
            && entries_[static_cast<size_t> (row)].isDir)
            setDirectory (entries_[static_cast<size_t> (row)].file);
        return true;
    }
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::backspaceKey)
    {
        goUp();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        if (onCancel) onCancel();
        return true;
    }
    return false;
}

void SampleBrowser::resized()
{
    auto area = getLocalBounds().reduced (10);

    pathLabel_.setBounds (area.removeFromTop (22));
    area.removeFromTop (2);
    hintLabel_.setBounds (area.removeFromTop (18));
    area.removeFromTop (6);

    auto footer = area.removeFromBottom (34);
    area.removeFromBottom (6);
    list_.setBounds (area);

    upButton_.setBounds (footer.removeFromLeft (60));
    footer.removeFromLeft (6);
    cancelButton_.setBounds (footer.removeFromRight (90));
    footer.removeFromRight (6);
    loadButton_.setBounds (footer.removeFromRight (150));
}

void SampleBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff232327));
}
