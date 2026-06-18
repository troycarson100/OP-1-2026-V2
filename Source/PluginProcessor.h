#pragma once

#include <atomic>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "core/Engine.h"
#include "modulation/ModTypes.h"

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
            case P::GrainSync:       suffix = "grainSync"; break;
            case P::GrainSteps:      suffix = "grainSteps"; break;
            case P::GrainPulses:     suffix = "grainPulses"; break;
            case P::GrainRotate:     suffix = "grainRotate"; break;
            case P::GrainPitchQuant: suffix = "grainPitchQuant"; break;
            case P::GrainPattern:        suffix = "grainPattern"; break;
            case P::GrainPatternAmount:  suffix = "grainPatternAmount"; break;
            case P::GrainContour:        suffix = "grainContour"; break;
            case P::GrainRandRev:        suffix = "grainRandRev"; break;
            case P::GrainRandAmp:        suffix = "grainRandAmp"; break;
            case P::FilterCutoff:    suffix = "filterCutoff"; break;
            case P::FilterResonance: suffix = "filterResonance"; break;
            case P::FilterMix:       suffix = "filterMix"; break;
            case P::FilterMode:      suffix = "filterMode"; break;
            case P::FilterDecay:     suffix = "filterDecay"; break;
            case P::FilterPitch:     suffix = "filterPitch"; break;
            case P::FilterScale:     suffix = "filterScale"; break;
            case P::ColorDrive:      suffix = "colorDrive"; break;
            case P::ColorCrush:      suffix = "colorCrush"; break;
            case P::ColorTilt:       suffix = "colorTilt"; break;
            case P::ColorCompress:   suffix = "colorCompress"; break;
            case P::ColorNoise:      suffix = "colorNoise"; break;
            case P::ColorNoiseDecay: suffix = "colorNoiseDecay"; break;
            case P::ColorNoiseTone:  suffix = "colorNoiseTone"; break;
            case P::ColorWet:        suffix = "colorWet"; break;
            case P::SpaceDelayAmount:   suffix = "spaceDelayAmt"; break;
            case P::SpaceDelayTime:     suffix = "spaceDelayTime"; break;
            case P::SpaceReverbAmount:  suffix = "spaceReverbAmt"; break;
            case P::SpaceReverbSize:    suffix = "spaceReverbSize"; break;
            case P::SpaceDelayFeedback: suffix = "spaceDelayFb"; break;
            case P::SpaceSpread:        suffix = "spaceSpread"; break;
            case P::SpaceDamp:          suffix = "spaceDamp"; break;
            case P::SpaceReverbDecay:   suffix = "spaceReverbDecay"; break;
            case P::SpaceDelayTimeMode: suffix = "spaceDelayTimeMode"; break;
            case P::SpaceFreeze:        suffix = "spaceFreeze"; break;
            case P::CaptureArm:      suffix = "captureArm"; break;
            case P::FilterKey:       suffix = "filterKey"; break;
            case P::MixEqLowGain:    suffix = "mixEqLow"; break;
            case P::MixEqMidGain:    suffix = "mixEqMid"; break;
            case P::MixEqHighGain:   suffix = "mixEqHigh"; break;
            case P::MixCompThreshold: suffix = "mixCompThr"; break;
            case P::MixCompMakeup:   suffix = "mixCompMakeup"; break;
            case P::MaterialWaveZoom: suffix = "materialWaveZoom"; break;
            case P::MaterialPlayhead: suffix = "materialPlayhead"; break;
            case P::MaterialTimeMode: suffix = "materialTimeMode"; break;
            case P::SampleRootBpm:   suffix = "sampleRootBpm"; break;
            case P::TapeSpeedSnap: suffix = "tapeSpeedSnap"; break;
            case P::LoopSnapGrid:  suffix = "loopSnapGrid"; break;
            case P::MaterialPitchScale: suffix = "materialPitchScale"; break;
            case P::MaterialPitchKey:   suffix = "materialPitchKey"; break;
            case P::MaterialLoopXfade:   suffix = "materialLoopXfade"; break;
            case P::MaterialGridDivision: suffix = "materialGridDivision"; break;
            default: break;
        }

        return "t" + juce::String (track + 1) + "_" + suffix;
    }
} // namespace bridge

// JUCE wrapper only: bridges host audio/MIDI/parameters/state to the
// portable sculpt::Engine. No DSP or instrument logic lives here.
class SculptSamplerAudioProcessor : public juce::AudioProcessor,
                                     private juce::AudioProcessorParameter::Listener
{
public:
    SculptSamplerAudioProcessor();
    ~SculptSamplerAudioProcessor() override;

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

    void requestSpaceClear (int trackIndex) { engine_.requestSpaceClear (trackIndex); }

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts_; }
    sculpt::Engine& getEngine()                              { return engine_; }

    sculpt::ModPatch getModPatch() const;
    void               setModPatch (const sculpt::ModPatch& patch);

    // LCD / UI: horizontal BPM drag on the instrument header sets this until cleared by preset
    // reload; saved in plugin state. When false, host tempo (if any) updates manualBpm each block.
    void applyUserBpm (double bpm);

    // Decodes WAV/AIFF/etc. into the selected track's material buffer (not RT).
    bool loadAudioFileIntoTrack (int trackIndex, const juce::File& file);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void buildParameterLinks();
    void syncParametersToEngine();
    void handleMidi (const juce::MidiBuffer& midi);

    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;

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

    mutable juce::CriticalSection modPatchLock_;
    sculpt::ModPatch              modPatch_;

    std::atomic<double> manualBpm_ { 120.0 };
    std::atomic<bool>    manualTempoOverride_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptSamplerAudioProcessor)
};
