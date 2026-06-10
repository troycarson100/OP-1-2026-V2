#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Engine.h"

// The one place that maps engine ParameterIds to host parameter ID strings.
namespace bridge
{
    inline juce::String paramIdString (int track, sculpt::ParameterId id)
    {
        using P = sculpt::ParameterId;

        if (! sculpt::isTrackParameter (id))
        {
            switch (id)
            {
                case P::OutputGain:    return "outputGain";
                case P::SelectedTrack: return "selectedTrack";
                case P::Macro1:        return "macro1";
                case P::Macro2:        return "macro2";
                case P::Macro3:        return "macro3";
                case P::Macro4:        return "macro4";
                default:               return "unknown";
            }
        }

        const char* suffix = "unknown";
        switch (id)
        {
            case P::TrackLevel:      suffix = "level"; break;
            case P::TrackPan:        suffix = "pan"; break;
            case P::MaterialLevel:   suffix = "materialLevel"; break;
            case P::TapeSpeed:       suffix = "tapeSpeed"; break;
            case P::LoopStart:       suffix = "loopStart"; break;
            case P::LoopEnd:         suffix = "loopEnd"; break;
            case P::GrainPosition:   suffix = "grainPosition"; break;
            case P::GrainSize:       suffix = "grainSize"; break;
            case P::GrainDensity:    suffix = "grainDensity"; break;
            case P::GrainPitch:      suffix = "grainPitch"; break;
            case P::GrainSpray:      suffix = "grainSpray"; break;
            case P::GrainTexture:    suffix = "grainTexture"; break;
            case P::GrainSpread:     suffix = "grainSpread"; break;
            case P::GrainMix:        suffix = "grainMix"; break;
            case P::FilterCutoff:    suffix = "filterCutoff"; break;
            case P::FilterResonance: suffix = "filterResonance"; break;
            case P::FilterMix:       suffix = "filterMix"; break;
            case P::ColorDrive:      suffix = "colorDrive"; break;
            case P::ColorTone:       suffix = "colorTone"; break;
            case P::ColorMix:        suffix = "colorMix"; break;
            case P::SpaceAmount:     suffix = "spaceAmount"; break;
            case P::SpaceFeedback:   suffix = "spaceFeedback"; break;
            case P::SpaceMix:        suffix = "spaceMix"; break;
            case P::CaptureArm:      suffix = "captureArm"; break;
            default: break;
        }

        return "t" + juce::String (track + 1) + "_" + suffix;
    }
} // namespace bridge

// JUCE wrapper only: bridges host audio/MIDI/parameters/state to the
// portable sculpt::Engine. No DSP or instrument logic lives here.
class SculptSamplerAudioProcessor : public juce::AudioProcessor
{
public:
    SculptSamplerAudioProcessor();
    ~SculptSamplerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                       { return true; }

    const juce::String getName() const override           { return JucePlugin_Name; }
    bool acceptsMidi() const override                     { return true; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override          { return 2.0; }

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts_; }
    sculpt::Engine& getEngine()                              { return engine_; }

    // Decodes WAV/AIFF/etc. into the selected track's material buffer (not RT).
    bool loadAudioFileIntoTrack (int trackIndex, const juce::File& file);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void buildParameterLinks();
    void syncParametersToEngine();
    void handleMidi (const juce::MidiBuffer& midi);

    struct ParameterLink
    {
        juce::RangedAudioParameter* parameter = nullptr;
        int track = -1;                                  // -1 = global
        sculpt::ParameterId id = sculpt::ParameterId::Count;
    };

    sculpt::Engine engine_;
    juce::AudioFormatManager formatManager_;
    juce::AudioProcessorValueTreeState apvts_;
    std::vector<ParameterLink> links_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptSamplerAudioProcessor)
};
