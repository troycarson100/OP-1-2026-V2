#pragma once

#include <functional>
#include <set>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

// Elektron-style sample browser: navigate the filesystem with the keyboard, mark multiple audio
// files (Enter/Space toggles), then load them all into the project bank at once. Temporary debug
// UI only — no engine logic lives here.
class SampleBrowser : public juce::Component,
                      private juce::ListBoxModel
{
public:
    explicit SampleBrowser (const juce::File& startDir);

    // Called with all marked files when the user confirms (LOAD). onCancel fires on cancel/close.
    std::function<void (const juce::Array<juce::File>&)> onLoad;
    std::function<void()>                                onCancel;

    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    // ListBoxModel
    int  getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void setDirectory (const juce::File& dir);
    void activateRow (int row);     // enter folder / parent, or toggle a file's mark
    void toggleMarkRow (int row);
    void goUp();
    void confirmLoad();

    struct Entry
    {
        juce::File file;
        bool       isDir    = false;
        bool       isParent = false;
    };

    // ListBox subclass: handle Up/Down itself, forward everything else (Enter/Left/Right/Space)
    // to the browser so folder navigation + marking work from the keyboard.
    struct KeyList : juce::ListBox
    {
        std::function<bool (const juce::KeyPress&)> onUnhandledKey;
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (juce::ListBox::keyPressed (k))
                return true;
            return onUnhandledKey ? onUnhandledKey (k) : false;
        }
    };

    juce::File                dir_;
    std::vector<Entry>        entries_;
    std::set<juce::String>    marked_;     // absolute paths of marked files

    KeyList          list_;
    juce::Label      pathLabel_;
    juce::Label      hintLabel_;
    juce::TextButton loadButton_   { "LOAD MARKED" };
    juce::TextButton cancelButton_ { "CANCEL" };
    juce::TextButton upButton_     { "UP" };

    static constexpr const char* kAudioExts = "*.wav;*.aif;*.aiff;*.flac;*.ogg";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleBrowser)
};
