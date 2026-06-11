#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "util/Constants.h"
#include "core/FilterScales.h"

#include <cstring>
#include <vector>

namespace
{
    // MIDI notes that trigger tracks 1-4 (two octaves for convenience).
    constexpr int kTriggerNoteLow  = 36;
    constexpr int kTriggerNoteHigh = 60;

    juce::StringArray makeFilterScaleChoices()
    {
        juce::StringArray a;
        for (int i = 0; i < static_cast<int> (sculpt::FilterScale::Count); ++i)
            a.add (sculpt::filterScaleName (static_cast<sculpt::FilterScale> (i)));
        return a;
    }

    juce::StringArray makeFilterKeyChoices()
    {
        juce::StringArray a;
        for (int i = 0; i < 12; ++i)
            a.add (sculpt::filterKeyName (i));
        return a;
    }

    juce::StringArray makeFilterModeChoices()
    {
        juce::StringArray a;
        a.add ("LP / BP / HP");
        a.add ("Ring");
        return a;
    }

    int choiceDefaultIndex (sculpt::ParameterId id, int numChoices)
    {
        if (numChoices < 2)
            return 0;
        const float d = sculpt::parameterDefault (id);
        return juce::jlimit (0, numChoices - 1,
                             juce::roundToInt (d * static_cast<float> (numChoices - 1)));
    }

    void addChoiceParam (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                         const juce::String& idString, const juce::String& name,
                         const juce::StringArray& choices, sculpt::ParameterId id)
    {
        const int n = choices.size();
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { idString, 1 }, name, choices, choiceDefaultIndex (id, n)));
    }
}

SculptSamplerAudioProcessor::SculptSamplerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_ (*this, nullptr, "PARAMS", createParameterLayout())
{
    formatManager_.registerBasicFormats();
    buildParameterLinks();
}

juce::AudioProcessorValueTreeState::ParameterLayout SculptSamplerAudioProcessor::createParameterLayout()
{
    using namespace sculpt;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addParam = [&layout] (const juce::String& idString, const juce::String& name, float defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { idString, 1 }, name,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), defaultValue));
    };

    const juce::StringArray scaleChoices = makeFilterScaleChoices();
    const juce::StringArray keyChoices   = makeFilterKeyChoices();
    const juce::StringArray modeChoices  = makeFilterModeChoices();

    for (int i = 0; i < kNumParameters; ++i)
    {
        const auto id = static_cast<ParameterId> (i);

        if (! isTrackParameter (id))
        {
            addParam (bridge::paramIdString (-1, id), parameterName (id), parameterDefault (id));
        }
        else
        {
            for (int t = 0; t < kNumTracks; ++t)
            {
                const juce::String pid = bridge::paramIdString (t, id);
                const juce::String pname = "T" + juce::String (t + 1) + " " + parameterName (id);

                if (id == ParameterId::FilterScale)
                    addChoiceParam (layout, pid, pname, scaleChoices, id);
                else if (id == ParameterId::FilterKey)
                    addChoiceParam (layout, pid, pname, keyChoices, id);
                else if (id == ParameterId::FilterMode)
                    addChoiceParam (layout, pid, pname, modeChoices, id);
                else
                    addParam (pid, pname, parameterDefault (id));
            }
        }
    }

    return layout;
}

void SculptSamplerAudioProcessor::buildParameterLinks()
{
    using namespace sculpt;
    links_.clear();

    for (int i = 0; i < kNumParameters; ++i)
    {
        const auto id = static_cast<ParameterId> (i);

        if (! isTrackParameter (id))
        {
            if (auto* p = apvts_.getParameter (bridge::paramIdString (-1, id)))
                links_.push_back ({ p, -1, id });
        }
        else
        {
            for (int t = 0; t < kNumTracks; ++t)
                if (auto* p = apvts_.getParameter (bridge::paramIdString (t, id)))
                    links_.push_back ({ p, t, id });
        }
    }
}

void SculptSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine_.prepare (sampleRate, samplesPerBlock);
    syncParametersToEngine();
    {
        const juce::ScopedLock sl (modPatchLock_);
        engine_.setModPatch (modPatch_);
    }
}

void SculptSamplerAudioProcessor::releaseResources()
{
    engine_.reset();
}

bool SculptSamplerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::mono()
        || in.isDisabled();
}

void SculptSamplerAudioProcessor::syncParametersToEngine()
{
    // Parameter values are normalized 0..1 on both sides, so getValue()
    // passes straight through with no extra conversion logic.
    for (const auto& link : links_)
    {
        if (link.track < 0)
            engine_.setParameter (link.id, link.parameter->getValue());
        else
            engine_.setTrackParameter (link.track, link.id, link.parameter->getValue());
    }
}

void SculptSamplerAudioProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (! message.isNoteOnOrOff())
            continue;

        const int note = message.getNoteNumber();
        int track = -1;
        if (note >= kTriggerNoteLow && note < kTriggerNoteLow + sculpt::kNumTracks)
            track = note - kTriggerNoteLow;
        else if (note >= kTriggerNoteHigh && note < kTriggerNoteHigh + sculpt::kNumTracks)
            track = note - kTriggerNoteHigh;

        if (track < 0)
            continue;

        if (message.isNoteOn())
            engine_.triggerTrack (track);
        else
            engine_.stopTrack (track);
    }
}

void SculptSamplerAudioProcessor::applyUserBpm (double bpm)
{
    const double clamped = juce::jlimit (32.0, 300.0, bpm);
    manualBpm_.store (clamped, std::memory_order_relaxed);
    manualTempoOverride_.store (true, std::memory_order_relaxed);
}

void SculptSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Host transport / tempo: follow play head unless user has dragged BPM (manual override).
    double     tempoOut = manualBpm_.load (std::memory_order_relaxed);
    const bool manualOn = manualTempoOverride_.load (std::memory_order_relaxed);
    bool       havePos  = false;
    bool       playing  = false;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            havePos = true;
            playing = position->getIsPlaying();

            if (! manualOn && position->getBpm())
            {
                tempoOut = static_cast<double> (*position->getBpm());
                manualBpm_.store (tempoOut, std::memory_order_relaxed);
            }
        }
    }

    engine_.setHostTempo (tempoOut);
    if (havePos)
        engine_.setHostPlaying (playing);

    handleMidi (midiMessages);
    midiMessages.clear();

    syncParametersToEngine();

    {
        const juce::ScopedLock sl (modPatchLock_);
        engine_.setModPatch (modPatch_);
    }

    // In-place: the engine reads inputs (capture/env-follow) before it
    // overwrites the same buffers with the mixed output.
    float* const* channels = buffer.getArrayOfWritePointers();
    engine_.process (const_cast<float**> (channels),
                     const_cast<float**> (channels),
                     getTotalNumInputChannels(),
                     getTotalNumOutputChannels(),
                     buffer.getNumSamples());
}

bool SculptSamplerAudioProcessor::loadAudioFileIntoTrack (int trackIndex, const juce::File& file)
{
    if (trackIndex < 0 || trackIndex >= sculpt::kNumTracks || ! file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager_.createReaderFor (file));
    if (reader == nullptr)
        return false;

    const juce::int64 srcFrames64 = reader->lengthInSamples;
    if (srcFrames64 < 1)
        return false;

    const double hostSr = getSampleRate() > 0.0 ? getSampleRate() : reader->sampleRate;
    const double fileSr  = reader->sampleRate > 0.0 ? reader->sampleRate : hostSr;
    const int    maxOut  = static_cast<int> (hostSr * static_cast<double> (sculpt::kMaxCaptureSeconds));

    const juce::int64 maxSrcToRead = static_cast<juce::int64> (std::ceil (fileSr * static_cast<double> (sculpt::kMaxCaptureSeconds) * 2.0));
    const int         srcRead      = static_cast<int> (juce::jmin (srcFrames64, maxSrcToRead));

    const int srcCh = juce::jlimit (1, 64, static_cast<int> (reader->numChannels));
    juce::AudioBuffer<float> srcBuf (srcCh, srcRead);
    reader->read (&srcBuf, 0, srcRead, 0, true, true);

    const int outFrames = juce::jmin (maxOut, static_cast<int> (std::ceil (static_cast<double> (srcRead) * hostSr / fileSr)));
    if (outFrames < 1)
        return false;

    std::vector<float> L (static_cast<size_t> (outFrames));
    std::vector<float> R (static_cast<size_t> (outFrames));

    const double stepSrc = fileSr / hostSr;   // source samples per output frame

    for (int i = 0; i < outFrames; ++i)
    {
        const double pos = static_cast<double> (i) * stepSrc;
        const int    i0  = static_cast<int> (pos);
        const int    i1  = juce::jmin (i0 + 1, srcRead - 1);
        const float  frac = static_cast<float> (pos - static_cast<double> (i0));

        const float l0 = srcBuf.getSample (0, i0);
        const float l1 = srcBuf.getSample (0, i1);
        L[static_cast<size_t> (i)] = l0 + (l1 - l0) * frac;

        const int rCh = srcCh > 1 ? 1 : 0;
        const float r0 = srcBuf.getSample (rCh, i0);
        const float r1 = srcBuf.getSample (rCh, i1);
        R[static_cast<size_t> (i)] = r0 + (r1 - r0) * frac;
    }

    suspendProcessing (true);
    engine_.replaceTrackMaterialStereo (trackIndex, L.data(), R.data(), outFrames);
    suspendProcessing (false);
    return true;
}

juce::AudioProcessorEditor* SculptSamplerAudioProcessor::createEditor()
{
    return new SculptSamplerAudioProcessorEditor (*this);
}

sculpt::ModPatch SculptSamplerAudioProcessor::getModPatch() const
{
    const juce::ScopedLock sl (modPatchLock_);
    return modPatch_;
}

void SculptSamplerAudioProcessor::setModPatch (const sculpt::ModPatch& patch)
{
    const juce::ScopedLock sl (modPatchLock_);
    modPatch_ = patch;
}

void SculptSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts_.copyState().createXml())
    {
        sculpt::ModPatch copy {};
        {
            const juce::ScopedLock sl (modPatchLock_);
            copy = modPatch_;
        }
        const juce::MemoryBlock raw (&copy, sizeof (copy));
        xml->setAttribute ("sculptModBlob", juce::Base64::toBase64 (raw.getData(), raw.getSize()));
        xml->setAttribute ("manualBpm", manualBpm_.load (std::memory_order_relaxed));
        xml->setAttribute ("manualTempoOverride", manualTempoOverride_.load (std::memory_order_relaxed) ? 1 : 0);
        copyXmlToBinary (*xml, destData);
    }
}

void SculptSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const juce::String b64 = xml->getStringAttribute ("sculptModBlob");

        if (xml->hasTagName (apvts_.state.getType()))
            apvts_.replaceState (juce::ValueTree::fromXml (*xml));

        manualBpm_.store (xml->getDoubleAttribute ("manualBpm", 120.0), std::memory_order_relaxed);
        manualTempoOverride_.store (xml->getIntAttribute ("manualTempoOverride", 0) != 0,
                                    std::memory_order_relaxed);

        juce::MemoryOutputStream mos;
        if (b64.isNotEmpty() && juce::Base64::convertFromBase64 (mos, b64))
        {
            if (mos.getDataSize() == sizeof (modPatch_))
            {
                const juce::ScopedLock sl (modPatchLock_);
                std::memcpy (&modPatch_, mos.getData(), sizeof (modPatch_));
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SculptSamplerAudioProcessor();
}
