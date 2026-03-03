#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
 * PresetManager -- Factory + User presets for Patina.
 *
 * Factory presets: 12 curated presets compiled into the binary.
 * User presets:    Saved as XML files in the user's app data folder.
 *
 * Signal chain: Noise -> Wobble -> Distort -> Resonator -> Space
 *
 * User preset directory:
 *   Windows: %APPDATA%\Futureproof Music School\Patina\Presets\
 *   macOS:   ~/Library/Application Support/Futureproof Music School/Patina/Presets/
 */
class PresetManager
{
public:
    struct Preset
    {
        const char* name;

        // Noise
        float noise;          // noise_amount
        float noiseTone;      // noise_tone
        int   noiseType;      // noise_type: 0=White, 1=Pink, 2=Brown

        // Wobble
        float wobble;         // wobble_amount
        float wobbleRate;     // wobble_rate

        // Distort
        float distort;        // distort_amount
        float distortTone;    // distort_tone
        int   distortType;    // distort_type: 0=Soft, 1=Hard, 2=Diode, 3=Fold, 4=Crush

        // Resonator
        float resonator;      // resonator_amount
        float resonatorFreq;  // resonator_freq
        float resonatorReso;  // resonator_reso
        int   resonatorType;  // resonator_type: 0=Comb, 1=Modal, 2=Formant

        // Space
        float space;          // space_amount
        float spaceDecay;     // space_decay

        // Master
        float mix;            // mix
    };

    static constexpr int kNumPresets = 12;
    static const Preset kPresets[kNumPresets];

    PresetManager();

    // ---- Factory preset access ----
    int         getCurrentIndex() const { return currentFactoryIndex; }
    const char* getCurrentName()  const;

    void loadPreset(int index, juce::AudioProcessorValueTreeState& apvts);
    void nextPreset(juce::AudioProcessorValueTreeState& apvts);
    void prevPreset(juce::AudioProcessorValueTreeState& apvts);

    // Restore index without applying params (used in setStateInformation)
    void setCurrentIndexOnly(int index);

    // ---- User preset management ----
    static juce::File getUserPresetsDir();

    /** Returns sorted list of user preset names (without .xml extension). */
    juce::StringArray getUserPresetNames() const;

    /** Save current APVTS state as a named user preset. Returns true on success. */
    bool saveUserPreset(const juce::String& name,
                        juce::AudioProcessorValueTreeState& apvts);

    /** Load a user preset by name. Returns true on success. */
    bool loadUserPreset(const juce::String& name,
                        juce::AudioProcessorValueTreeState& apvts);

    /** Delete a user preset by name. Returns true on success. */
    bool deleteUserPreset(const juce::String& name);

    // ---- State tracking ----
    bool isUserPreset() const { return currentIsUser; }
    juce::String getCurrentUserPresetName() const { return currentUserName; }

    /** Mark state as "modified" (user tweaked knobs after loading). */
    void markModified() { modified = true; }
    bool isModified() const { return modified; }

    /** Total navigable presets (factory + user). */
    int getTotalPresetCount() const;

    /** Navigate through all presets (factory first, then user alphabetically). */
    void nextPresetGlobal(juce::AudioProcessorValueTreeState& apvts);
    void prevPresetGlobal(juce::AudioProcessorValueTreeState& apvts);

    /** Get display name for current preset. */
    juce::String getDisplayName() const;

    // ---- Parameter IDs for serialization ----
    static const char* const kParamIds[];
    static constexpr int kNumParams = 15;

private:
    int currentFactoryIndex = 0;
    bool currentIsUser = false;
    juce::String currentUserName;
    bool modified = false;

    static void applyPreset(const Preset& p, juce::AudioProcessorValueTreeState& apvts);
    static void setParam(juce::AudioProcessorValueTreeState& apvts, const char* id, float value);

    /** Get File object for a user preset name. */
    static juce::File getUserPresetFile(const juce::String& name);
};
