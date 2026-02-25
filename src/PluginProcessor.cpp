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

    // Module amount parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "noise_amount", 1 }, "Noise", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "wobble_amount", 1 }, "Wobble", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "distort_amount", 1 }, "Distort", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "space_amount", 1 }, "Space", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "flux_amount", 1 }, "Flux", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "filter_amount", 1 }, "Filter", 0.0f, 1.0f, 0.5f));

    // Secondary module parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "wobble_rate", 1 }, "Wobble Rate",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "space_decay", 1 }, "Space Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    // Master controls
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
double PatinaProcessor::getTailLengthSeconds() const { return 0.5; } // Reverb tail

int PatinaProcessor::getNumPrograms()    { return 1; }
int PatinaProcessor::getCurrentProgram() { return 0; }
void PatinaProcessor::setCurrentProgram(int) {}
const juce::String PatinaProcessor::getProgramName(int) { return {}; }
void PatinaProcessor::changeProgramName(int, const juce::String&) {}

void PatinaProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Prepare all DSP modules
    noiseModule.prepare(sampleRate, samplesPerBlock);
    wobbleModule.prepare(sampleRate, samplesPerBlock);
    distortModule.prepare(sampleRate, samplesPerBlock);
    spaceModule.prepare(sampleRate, samplesPerBlock);
    fluxModule.prepare(sampleRate, samplesPerBlock);
    filterModule.prepare(sampleRate, samplesPerBlock);

    // Reset FFT state
    fftInputBuffer.fill(0.0f);
    fftOutputBuffer.fill(0.0f);
    fftWriteIndex = 0;
}

void PatinaProcessor::releaseResources()
{
    // Reset all modules
    noiseModule.reset();
    wobbleModule.reset();
    distortModule.reset();
    spaceModule.reset();
    fluxModule.reset();
    filterModule.reset();
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

    // Clear any unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // ---- Read all parameters ----
    float noiseAmount   = apvts.getRawParameterValue("noise_amount")->load();
    float wobbleAmount  = apvts.getRawParameterValue("wobble_amount")->load();
    float distortAmount = apvts.getRawParameterValue("distort_amount")->load();
    float spaceAmount   = apvts.getRawParameterValue("space_amount")->load();
    float fluxAmount    = apvts.getRawParameterValue("flux_amount")->load();
    float filterAmount  = apvts.getRawParameterValue("filter_amount")->load();
    float wobbleRate    = apvts.getRawParameterValue("wobble_rate")->load();
    float spaceDecay    = apvts.getRawParameterValue("space_decay")->load();
    float mix           = apvts.getRawParameterValue("mix")->load();

    // ---- Save dry signal for mix ----
    juce::AudioBuffer<float> dryBuffer;
    if (mix < 0.999f)
    {
        dryBuffer.makeCopyOf(buffer);
    }

    // ---- Update Flux modulation ----
    // Flux generates random modulation signals that affect other modules
    // Process updates modulation values AND applies amplitude flutter
    fluxModule.process(buffer, fluxAmount);

    // Get modulation offsets from Flux
    float filterMod = fluxModule.getFilterMod();
    float wobbleMod = fluxModule.getWobbleMod();
    float noiseMod  = fluxModule.getNoiseMod();

    // ---- Signal Chain ----
    // Order: Noise (additive) → Wobble (delay) → Filter → Distort → Space
    //
    // Noise first: adds texture before everything else
    // Wobble second: pitch warping on the textured signal
    // Filter third: shapes frequency content before distortion
    //   (filtering before distortion controls which harmonics are generated)
    // Distort fourth: waveshaping on the filtered signal
    // Space last: reverb on the final processed signal
    //   (distortion before reverb sounds more natural than reverb before distortion)

    // 1. NOISE — additive: mix noise into the signal
    if (noiseAmount > 0.001f)
    {
        float modulatedNoise = std::min(noiseAmount + noiseMod * 0.3f, 1.0f);
        noiseModule.process(buffer, modulatedNoise);
    }

    // 2. WOBBLE — volume tremolo with rate control
    if (wobbleAmount > 0.001f)
    {
        float modulatedWobble = std::min(wobbleAmount + wobbleMod * 0.2f, 1.0f);
        wobbleModule.process(buffer, modulatedWobble, wobbleRate);
    }

    // 3. FILTER — frequency shaping (always processes, centered at 0.5 = flat)
    filterModule.process(buffer, filterAmount, filterMod);

    // 4. DISTORT — saturation and bit crushing
    if (distortAmount > 0.001f)
    {
        distortModule.process(buffer, distortAmount);
    }

    // 5. SPACE — plate reverb with independent decay
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
    // Prevents any edge-case amplitude overages from reaching the DAW output.
    // At unity gain this loop never fires; it's a last-resort guard.
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
    // Push output samples into a circular buffer for FFT analysis
    {
        auto numSamples = buffer.getNumSamples();
        auto* leftChannel = buffer.getReadPointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            fftInputBuffer[static_cast<size_t>(fftWriteIndex)] = leftChannel[i];

            if (++fftWriteIndex >= FFT_SIZE)
            {
                fftWriteIndex = 0;

                // Copy input to output buffer for FFT
                std::copy(fftInputBuffer.begin(),
                          fftInputBuffer.begin() + FFT_SIZE,
                          fftOutputBuffer.begin());

                // Apply Hann window
                for (int j = 0; j < FFT_SIZE; ++j)
                {
                    float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                            * static_cast<float>(j)
                                                            / static_cast<float>(FFT_SIZE)));
                    fftOutputBuffer[static_cast<size_t>(j)] *= window;
                }

                // Perform FFT
                fft.performFrequencyOnlyForwardTransform(fftOutputBuffer.data());
            }
        }
    }
}

void PatinaProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Save lock state and preset index into the same ValueTree
    randomizeSystem.saveToValueTree(state);
    state.setProperty("presetIndex", presetManager.getCurrentIndex(), nullptr);

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

            // Restore lock state and preset index before replacing APVTS state
            randomizeSystem.loadFromValueTree(newState);
            presetManager.setCurrentIndexOnly(
                static_cast<int>(newState.getProperty("presetIndex", 0)));

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
