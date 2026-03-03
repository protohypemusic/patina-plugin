#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

PatinaProcessor::PatinaProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

PatinaProcessor::~PatinaProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PatinaProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ---- Noise ----
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "noise_amount", 1 }, "Noise", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "noise_tone", 1 }, "Noise Tone", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ "noise_type", 1 }, "Noise Type", 0, 2, 0));  // White/Pink/Brown

    // ---- Wobble ----
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "wobble_amount", 1 }, "Wobble", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "wobble_rate", 1 }, "Wobble Rate",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    // ---- Distort ----
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "distort_amount", 1 }, "Distort", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "distort_tone", 1 }, "Distort Tone", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ "distort_type", 1 }, "Distort Type", 0, 4, 0));  // Soft/Hard/Diode/Fold/Crush

    // ---- Resonator ----
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "resonator_amount", 1 }, "Resonator", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "resonator_freq", 1 }, "Resonator Freq", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "resonator_reso", 1 }, "Resonator Reso", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ "resonator_type", 1 }, "Resonator Type", 0, 2, 0));  // Comb/Modal/Formant

    // ---- Space ----
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "space_amount", 1 }, "Space", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "space_decay", 1 }, "Space Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    // ---- Master ----
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "mix", 1 }, "Mix", 0.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

const juce::String PatinaProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PatinaProcessor::acceptsMidi() const  { return false; }
bool PatinaProcessor::producesMidi() const { return false; }
bool PatinaProcessor::isMidiEffect() const { return false; }
double PatinaProcessor::getTailLengthSeconds() const { return 0.5; }

int PatinaProcessor::getNumPrograms()    { return 1; }
int PatinaProcessor::getCurrentProgram() { return 0; }
void PatinaProcessor::setCurrentProgram(int) {}
const juce::String PatinaProcessor::getProgramName(int) { return {}; }
void PatinaProcessor::changeProgramName(int, const juce::String&) {}

void PatinaProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    noiseModule.prepare(sampleRate, samplesPerBlock);
    wobbleModule.prepare(sampleRate, samplesPerBlock);
    distortModule.prepare(sampleRate, samplesPerBlock);
    resonatorModule.prepare(sampleRate, samplesPerBlock);
    spaceModule.prepare(sampleRate, samplesPerBlock);

    fftInputBuffer.fill(0.0f);
    fftOutputBuffer.fill(0.0f);
    fftWriteIndex = 0;
}

void PatinaProcessor::releaseResources()
{
    noiseModule.reset();
    wobbleModule.reset();
    distortModule.reset();
    resonatorModule.reset();
    spaceModule.reset();
}

bool PatinaProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    return true;
}

void PatinaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // ---- Read parameters ----
    float noiseAmount     = apvts.getRawParameterValue("noise_amount")->load();
    float noiseTone       = apvts.getRawParameterValue("noise_tone")->load();
    int   noiseType       = static_cast<int>(apvts.getRawParameterValue("noise_type")->load());

    float wobbleAmount    = apvts.getRawParameterValue("wobble_amount")->load();
    float wobbleRate      = apvts.getRawParameterValue("wobble_rate")->load();

    float distortAmount   = apvts.getRawParameterValue("distort_amount")->load();
    float distortTone     = apvts.getRawParameterValue("distort_tone")->load();
    int   distortType     = static_cast<int>(apvts.getRawParameterValue("distort_type")->load());

    float resonatorAmount = apvts.getRawParameterValue("resonator_amount")->load();
    float resonatorFreq   = apvts.getRawParameterValue("resonator_freq")->load();
    float resonatorReso   = apvts.getRawParameterValue("resonator_reso")->load();
    int   resonatorType   = static_cast<int>(apvts.getRawParameterValue("resonator_type")->load());

    float spaceAmount     = apvts.getRawParameterValue("space_amount")->load();
    float spaceDecay      = apvts.getRawParameterValue("space_decay")->load();

    float mix             = apvts.getRawParameterValue("mix")->load();

    // ---- Save dry signal for mix ----
    juce::AudioBuffer<float> dryBuffer;
    if (mix < 0.999f)
    {
        dryBuffer.makeCopyOf(buffer);
    }

    // ---- Signal Chain: Noise -> Wobble -> Distort -> Resonator -> Space ----

    // 1. NOISE -- adds texture to the signal
    noiseModule.process(buffer, noiseAmount, noiseTone, noiseType);

    // 2. WOBBLE -- volume tremolo
    if (wobbleAmount > 0.001f)
    {
        wobbleModule.process(buffer, wobbleAmount, wobbleRate);
    }

    // 3. DISTORT -- waveshaping with post-EQ
    distortModule.process(buffer, distortAmount, distortTone, distortType);

    // 4. RESONATOR -- comb/modal/formant coloring
    resonatorModule.process(buffer, resonatorAmount, resonatorFreq, resonatorReso, resonatorType);

    // 5. SPACE -- plate reverb
    if (spaceAmount > 0.001f)
    {
        spaceModule.process(buffer, spaceAmount, spaceDecay);
    }

    // ---- Mix dry/wet ----
    if (mix < 0.999f)
    {
        auto numSamples = buffer.getNumSamples();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            auto* dry = dryBuffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                wet[i] = dry[i] * (1.0f - mix) + wet[i] * mix;
            }
        }
    }

    // ---- Hard output clamp (safety net) ----
    {
        auto numSamples = buffer.getNumSamples();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = juce::jlimit(-1.0f, 1.0f, data[i]);
        }
    }

    // ---- Feed FFT for visualizer ----
    {
        auto numSamples = buffer.getNumSamples();
        auto* leftChannel = buffer.getReadPointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            fftInputBuffer[static_cast<size_t>(fftWriteIndex)] = leftChannel[i];

            if (++fftWriteIndex >= FFT_SIZE)
            {
                fftWriteIndex = 0;

                std::copy(fftInputBuffer.begin(),
                          fftInputBuffer.begin() + FFT_SIZE,
                          fftOutputBuffer.begin());

                for (int j = 0; j < FFT_SIZE; ++j)
                {
                    float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                            * static_cast<float>(j)
                                                            / static_cast<float>(FFT_SIZE)));
                    fftOutputBuffer[static_cast<size_t>(j)] *= window;
                }

                fft.performFrequencyOnlyForwardTransform(fftOutputBuffer.data());
            }
        }
    }
}

void PatinaProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    randomizeSystem.saveToValueTree(state);
    state.setProperty("presetIndex", presetManager.getCurrentIndex(), nullptr);
    state.setProperty("isUserPreset", presetManager.isUserPreset(), nullptr);
    state.setProperty("userPresetName", presetManager.getCurrentUserPresetName(), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PatinaProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml(*xmlState);

            randomizeSystem.loadFromValueTree(newState);
            presetManager.setCurrentIndexOnly(
                static_cast<int>(newState.getProperty("presetIndex", 0)));

            // Restore user preset tracking (if a user preset was loaded when saved)
            bool wasUserPreset = static_cast<bool>(newState.getProperty("isUserPreset", false));
            juce::String userName = newState.getProperty("userPresetName", "").toString();

            if (wasUserPreset && userName.isNotEmpty())
            {
                // Try to reload the user preset to verify it still exists
                if (!presetManager.loadUserPreset(userName, apvts))
                {
                    // User preset file was deleted -- fall back to factory
                    presetManager.setCurrentIndexOnly(0);
                }
            }

            apvts.replaceState(newState);
        }
    }
}

juce::AudioProcessorEditor* PatinaProcessor::createEditor()
{
    return new PatinaEditor(*this);
}

bool PatinaProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PatinaProcessor();
}
