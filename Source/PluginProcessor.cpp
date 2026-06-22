#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "util/Constants.h"
#include "core/FilterScales.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
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

    juce::StringArray makeSpaceDelayTimeModeChoices()
    {
        juce::StringArray a;
        a.add ("Straight");
        a.add ("Dotted");
        a.add ("Triplet");
        a.add ("Free");
        return a;
    }

    juce::StringArray makeSampleModeChoices()
    {
        juce::StringArray a;
        a.add ("One-Shot");
        a.add ("Loop Fwd");
        a.add ("Loop Rev");
        a.add ("Loop Fwd/Back");
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

SculptSamplerAudioProcessor::~SculptSamplerAudioProcessor()
{
    for (const auto& link : links_)
        if (link.id == sculpt::ParameterId::MaterialPlayhead && link.parameter != nullptr)
            link.parameter->removeListener (this);
}

void SculptSamplerAudioProcessor::parameterValueChanged (int parameterIndex, float newValue)
{
    juce::ignoreUnused (parameterIndex, newValue);
}

void SculptSamplerAudioProcessor::parameterGestureChanged (int parameterIndex, bool gestureIsStarting)
{
    using P = sculpt::ParameterId;
    for (const auto& link : links_)
    {
        if (link.id != P::MaterialPlayhead || link.track < 0 || link.parameter == nullptr)
            continue;
        if (link.parameter->getParameterIndex() != parameterIndex)
            continue;
        engine_.setMaterialPlayheadScrubActive (link.track, gestureIsStarting);
        return;
    }
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
    const juce::StringArray spaceTimeModeChoices = makeSpaceDelayTimeModeChoices();
    const juce::StringArray sampleModeChoices     = makeSampleModeChoices();

    for (int i = 0; i < kNumParameters; ++i)
    {
        const auto id = static_cast<ParameterId> (i);

        if (! isTrackParameter (id))
        {
            const juce::String gid   = bridge::paramIdString (-1, id);
            const juce::String gname = parameterName (id);
            if (id == ParameterId::GlobalDelayTimeMode)
                addChoiceParam (layout, gid, gname, spaceTimeModeChoices, id);
            else if (id == ParameterId::GlobalSpaceFreeze)
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { gid, 1 }, gname, sculpt::parameterDefault (id) > 0.5f));
            else
                addParam (gid, gname, parameterDefault (id));
        }
        else
        {
            for (int t = 0; t < kNumTracks; ++t)
            {
                const juce::String pid = bridge::paramIdString (t, id);
                const juce::String pname = "T" + juce::String (t + 1) + " " + parameterName (id);

                if (id == ParameterId::FilterScale || id == ParameterId::MaterialPitchScale)
                    addChoiceParam (layout, pid, pname, scaleChoices, id);
                else if (id == ParameterId::FilterKey || id == ParameterId::MaterialPitchKey)
                    addChoiceParam (layout, pid, pname, keyChoices, id);
                else if (id == ParameterId::FilterMode)
                    addChoiceParam (layout, pid, pname, modeChoices, id);
                else if (id == ParameterId::SpaceDelayTimeMode)
                    addChoiceParam (layout, pid, pname, spaceTimeModeChoices, id);
                else if (id == ParameterId::TapeSpeedSnap || id == ParameterId::LoopSnapGrid
                         || id == ParameterId::LoopStartFollow)
                    layout.add (std::make_unique<juce::AudioParameterBool> (
                        juce::ParameterID { pid, 1 }, pname, sculpt::parameterDefault (id) > 0.5f));
                else if (id == ParameterId::SpaceFreeze || id == ParameterId::MaterialMachine)
                    layout.add (std::make_unique<juce::AudioParameterBool> (
                        juce::ParameterID { pid, 1 }, pname, sculpt::parameterDefault (id) > 0.5f));
                else if (id == ParameterId::MaterialSampleMode)
                    addChoiceParam (layout, pid, pname, sampleModeChoices, id);
                else if (id == ParameterId::SampleRootBpm)
                {
                    auto attrs =
                        juce::AudioParameterFloatAttributes()
                            .withStringFromValueFunction ([] (float v, int)
                            {
                                return juce::String (juce::roundToInt (sculpt::map::sampleRootBpm (v)));
                            })
                            .withValueFromStringFunction ([] (const juce::String& text) -> float
                            {
                                const int bpm = juce::jlimit (40, 240, text.getIntValue());
                                return (static_cast<float> (bpm) - 40.0f) / 200.0f;
                            });
                    layout.add (std::make_unique<juce::AudioParameterFloat> (
                        juce::ParameterID { pid, 1 }, pname,
                        juce::NormalisableRange<float> (0.0f, 1.0f, 1.0f / 200.0f),
                        sculpt::parameterDefault (id), std::move (attrs)));
                }
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

    for (const auto& link : links_)
        if (link.id == ParameterId::MaterialPlayhead && link.parameter != nullptr)
            link.parameter->addListener (this);
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

// Decode + resample one file and append it to the project bank. Returns its slot index, or -1.
// Caller must suspend audio processing around this (it mutates the bank).
int SculptSamplerAudioProcessor::appendFileToBank (const juce::File& file)
{
    if (! file.existsAsFile())
        return -1;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager_.createReaderFor (file));
    if (reader == nullptr)
        return -1;

    const juce::int64 srcFrames64 = reader->lengthInSamples;
    if (srcFrames64 < 1)
        return -1;

    const double hostSr = getSampleRate() > 0.0 ? getSampleRate() : reader->sampleRate;
    const double fileSr = reader->sampleRate > 0.0 ? reader->sampleRate : hostSr;
    const double importSec = static_cast<double> (sculpt::kMaxImportSeconds);

    const juce::int64 maxSrcFrames = static_cast<juce::int64> (std::ceil (fileSr * importSec));
    const juce::int64 srcToRead64  = std::min (srcFrames64, maxSrcFrames);
    if (srcToRead64 < 1)
        return -1;

    const juce::int64 maxHostFrames64 = static_cast<juce::int64> (std::ceil (hostSr * importSec));
    const juce::int64 outFrames64     = static_cast<juce::int64> (std::ceil (static_cast<double> (srcToRead64) * hostSr / fileSr));
    const juce::int64 outCapped64     = std::min (outFrames64, maxHostFrames64);
    if (outCapped64 < 1
        || outCapped64 > static_cast<juce::int64> (std::numeric_limits<int>::max () - 4))
        return -1;

    const int srcRead = static_cast<int> (std::min (srcToRead64,
                                                    static_cast<juce::int64> (std::numeric_limits<int>::max () - 4)));
    const int outFrames = static_cast<int> (outCapped64);

    const int srcCh = juce::jlimit (1, 64, static_cast<int> (reader->numChannels));
    juce::AudioBuffer<float> srcBuf (srcCh, srcRead);
    reader->read (&srcBuf, 0, srcRead, 0, true, true);

    if (outFrames < 1)
        return -1;

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

    const juce::String stem = file.getFileNameWithoutExtension();
    return engine_.addBankSample (L.data(), R.data(), outFrames, stem.toRawUTF8());
}

// Point a track's Sample knob at an absolute bank slot. The knob scans only loaded samples
// (dense slots 0..count-1), so the normalized value is slot / (count-1).
void SculptSamplerAudioProcessor::pointTrackAtBankSlot (int trackIndex, int slot)
{
    if (slot < 0)
        return;
    const int count = engine_.getBankSampleCount();
    const float slotNorm = (count > 1)
        ? static_cast<float> (slot) / static_cast<float> (count - 1) : 0.0f;
    if (auto* p = apvts_.getParameter (bridge::paramIdString (trackIndex, sculpt::ParameterId::MaterialSampleSlot)))
        p->setValueNotifyingHost (slotNorm);
}

void SculptSamplerAudioProcessor::setActiveSampleRootBpm (int trackIndex, float bpm)
{
    const int slot = engine_.getActiveBankSlotForTrack (trackIndex);
    if (slot >= 0)
        engine_.setBankSampleRootBpm (slot, bpm);
}

bool SculptSamplerAudioProcessor::loadAudioFileIntoTrack (int trackIndex, const juce::File& file)
{
    if (trackIndex < 0 || trackIndex >= sculpt::kNumTracks)
        return false;

    suspendProcessing (true);
    const int slot = appendFileToBank (file);
    suspendProcessing (false);

    if (slot >= 0)
        pointTrackAtBankSlot (trackIndex, slot);
    return slot >= 0;
}

int SculptSamplerAudioProcessor::loadAudioFilesIntoBank (const juce::Array<juce::File>& files, int trackToAssign)
{
    suspendProcessing (true);
    int firstSlot = -1;
    int loaded    = 0;
    for (const auto& f : files)
    {
        const int slot = appendFileToBank (f);
        if (slot >= 0)
        {
            if (firstSlot < 0)
                firstSlot = slot;
            ++loaded;
        }
    }
    suspendProcessing (false);

    if (firstSlot >= 0 && trackToAssign >= 0 && trackToAssign < sculpt::kNumTracks)
        pointTrackAtBankSlot (trackToAssign, firstSlot);
    return loaded;
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

        // Portable performance state (sequencer patterns + p-locks, scenes, mutes) — gzip'd because
        // the raw pattern bank is ~1 MB of mostly-empty steps; compressed it's a few KB.
        auto writeBlob = [&xml] (const juce::String& attr, const void* src, size_t bytes)
        {
            juce::MemoryBlock packed;
            {
                juce::MemoryOutputStream mos (packed, false);
                juce::GZIPCompressorOutputStream gz (mos, 9);
                gz.write (src, bytes);
            }
            xml->setAttribute (attr, juce::Base64::toBase64 (packed.getData(), packed.getSize()));
            xml->setAttribute (attr + "Size", static_cast<int> (bytes));
        };

        std::vector<uint8_t> patBlob (engine_.patternStateBytes());
        engine_.savePatternState (patBlob.data());
        writeBlob ("sculptPatternBlob", patBlob.data(), patBlob.size());

        std::vector<uint8_t> sceneBlob (engine_.sceneStateBytes());
        engine_.saveSceneState (sceneBlob.data());
        writeBlob ("sculptSceneBlob", sceneBlob.data(), sceneBlob.size());

        // Loaded bank samples (raw float PCM, channel-major) so a project reopens with the exact
        // audio it had. gzip'd by writeBlob; this is a bounded message-thread copy, never the RT path.
        {
            std::vector<uint8_t> bank;
            auto put  = [&bank] (const void* p, size_t n)
            {
                const auto* b = static_cast<const uint8_t*> (p);
                bank.insert (bank.end(), b, b + n);
            };
            auto putI = [&put] (int32_t v) { put (&v, sizeof (v)); };
            auto putF = [&put] (float v)   { put (&v, sizeof (v)); };

            const size_t countPos = bank.size();
            putI (0); // patched with the real slot count below

            int32_t numWritten = 0;
            const int count = engine_.getBankSampleCount();
            for (int slot = 0; slot < count; ++slot)
            {
                if (! engine_.isBankSampleLoaded (slot))
                    continue;
                const int frames = engine_.getBankSampleFrames (slot);
                const int chans  = std::max (1, engine_.getBankSampleChannels (slot));
                if (frames <= 0)
                    continue;

                const char*   nm      = engine_.getBankSampleName (slot);
                const int32_t nameLen = nm != nullptr ? static_cast<int32_t> (std::strlen (nm)) : 0;

                putI (slot);
                putI (frames);
                putI (chans);
                putF (engine_.getBankSampleRootBpm (slot));
                putI (nameLen);
                if (nameLen > 0)
                    put (nm, static_cast<size_t> (nameLen));
                for (int ch = 0; ch < chans; ++ch)
                    if (const float* d = engine_.getBankSampleChannelData (slot, ch))
                        put (d, static_cast<size_t> (frames) * sizeof (float));
                ++numWritten;
            }
            std::memcpy (bank.data() + countPos, &numWritten, sizeof (numWritten));
            writeBlob ("sculptBankBlob", bank.data(), bank.size());
        }

        xml->setAttribute ("sculptMuteMask", static_cast<int> (engine_.getMuteMask()));

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

        // Restore portable performance state (patterns/p-locks, scenes) — decompress then memcpy
        // back into the engine, but only if the saved raw size matches this build's layout.
        auto readBlob = [&xml] (const juce::String& attr, std::vector<uint8_t>& out) -> bool
        {
            const juce::String b = xml->getStringAttribute (attr);
            const int rawSize     = xml->getIntAttribute (attr + "Size", 0);
            if (b.isEmpty() || rawSize <= 0)
                return false;

            juce::MemoryOutputStream packed;
            if (! juce::Base64::convertFromBase64 (packed, b))
                return false;

            juce::MemoryInputStream in (packed.getData(), packed.getDataSize(), false);
            juce::GZIPDecompressorInputStream gz (in);
            out.assign (static_cast<size_t> (rawSize), 0);
            return gz.read (out.data(), rawSize) == rawSize;
        };

        std::vector<uint8_t> blob;
        if (readBlob ("sculptPatternBlob", blob))
            engine_.loadPatternState (blob.data(), blob.size());
        if (readBlob ("sculptSceneBlob", blob))
            engine_.loadSceneState (blob.data(), blob.size());

        // Rebuild the sample pool from the saved PCM. Suspend audio: clearBank()/loadBankSlot()
        // reallocate buffers the audio thread reads from.
        std::vector<uint8_t> bankBlob;
        if (readBlob ("sculptBankBlob", bankBlob))
        {
            const uint8_t* p   = bankBlob.data();
            const uint8_t* end = p + bankBlob.size();
            auto getI = [&p, end] (int32_t& v) -> bool
            {
                if (p + sizeof (v) > end) return false;
                std::memcpy (&v, p, sizeof (v)); p += sizeof (v); return true;
            };
            auto getF = [&p, end] (float& v) -> bool
            {
                if (p + sizeof (v) > end) return false;
                std::memcpy (&v, p, sizeof (v)); p += sizeof (v); return true;
            };

            suspendProcessing (true);
            engine_.clearBank();

            int32_t num = 0;
            if (getI (num))
            {
                for (int i = 0; i < num; ++i)
                {
                    int32_t slot = 0, frames = 0, chans = 0, nameLen = 0;
                    float   rootBpm = 120.0f;
                    if (! (getI (slot) && getI (frames) && getI (chans) && getF (rootBpm) && getI (nameLen)))
                        break;
                    if (slot < 0 || frames <= 0 || chans < 1 || nameLen < 0
                        || p + nameLen > end)
                        break;

                    char name[64] = {};
                    std::memcpy (name, p, static_cast<size_t> (std::min (nameLen, 63)));
                    p += nameLen;

                    const size_t chBytes = static_cast<size_t> (frames) * sizeof (float);
                    if (p + static_cast<size_t> (chans) * chBytes > end)
                        break;

                    std::vector<float> L (static_cast<size_t> (frames));
                    std::memcpy (L.data(), p, chBytes);
                    std::vector<float> R;
                    const float* rptr = nullptr;
                    if (chans > 1)
                    {
                        R.resize (static_cast<size_t> (frames));
                        std::memcpy (R.data(), p + chBytes, chBytes);
                        rptr = R.data();
                    }
                    p += static_cast<size_t> (chans) * chBytes;

                    engine_.loadBankSlot (slot, L.data(), rptr, frames, name, rootBpm);
                }
            }
            suspendProcessing (false);
        }

        engine_.setMuteMask (static_cast<uint32_t> (xml->getIntAttribute ("sculptMuteMask", 0)));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SculptSamplerAudioProcessor();
}
