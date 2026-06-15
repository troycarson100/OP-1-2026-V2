#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Shared palette for S-4-inspired hardware panel (JUCE layer only).
namespace sculpt_editor
{
    inline const juce::Colour kBackground { 0xff16181d };
    inline const juce::Colour kPanel      { 0xff20242c };
    inline const juce::Colour kLcdBezel { 0xff12141a };
    inline const juce::Colour kAccent   { 0xffe8734a };
    inline const juce::Colour kMeter    { 0xff5fb89a };
    inline const juce::Colour kText     { 0xffd8dadf };

    // Material LCD waveform (high contrast, S-4-inspired)
    inline const juce::Colour kLcdWaveformBg { 0xff050608 };
    inline const juce::Colour kWaveformFill { 0xff6ed0d0 };
    inline const juce::Colour kWaveformStroke { 0xff9ee8e8 };
    inline const juce::Colour kWaveformLoopShade { 0x4020a0a0 };
    inline const juce::Colour kWaveformPlayhead { 0xffffffff };
    inline const juce::Colour kModLcdCarrierFill { 0xff4a6a72 };
    inline const juce::Colour kModLcdCarrierStroke { 0xff6a8a92 };
    inline const juce::Colour kModLcdModFill { 0x55e8734a };
    inline const juce::Colour kModLcdModStroke { 0xffe8734a };
    inline const juce::Colour kModLcdScanner { 0xffffffff };
    inline const juce::Colour kModLcdDot { 0xffffcc66 };

    // Parameter grid: live modulation hint (LCD value cells)
    inline const juce::Colour kParamModDot { 0xff4a9fff };
    inline const juce::Colour kParamModBarBg { 0xff2a2e36 };
    inline const juce::Colour kParamModThumb { 0xfff5f5f5 };
}
