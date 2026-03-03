#include "PresetManager.h"

// ================================================================
// Factory Presets
//
// Ordered from subtle to extreme for a nice browsing experience.
// Float values are raw [0, 1] matching APVTS parameter ranges.
// Tone: 0 = dark, 0.5 = flat, 1 = bright
// Type ints: see header for enum mappings.
// ================================================================

const PresetManager::Preset PresetManager::kPresets[PresetManager::kNumPresets] =
{
    //                          noise  nTone nTyp  wob   wRate  dist  dTone dTyp  reso  rFreq rReso rTyp  space sDecay mix
    { "Init",                   0.00f, 0.50f, 0,  0.00f, 0.30f, 0.00f, 0.50f, 0,  0.00f, 0.30f, 0.30f, 0,  0.00f, 0.30f, 1.00f },
    { "Subtle Warmth",          0.10f, 0.40f, 1,  0.15f, 0.25f, 0.04f, 0.45f, 0,  0.00f, 0.30f, 0.30f, 0,  0.08f, 0.25f, 1.00f },
    { "Tape Hiss",              0.35f, 0.42f, 1,  0.25f, 0.35f, 0.05f, 0.48f, 0,  0.00f, 0.30f, 0.30f, 0,  0.12f, 0.30f, 1.00f },
    { "Lo-Fi Radio",            0.40f, 0.35f, 0,  0.08f, 0.20f, 0.15f, 0.38f, 4,  0.15f, 0.45f, 0.25f, 2,  0.05f, 0.20f, 0.95f },
    { "Cassette Dreams",        0.30f, 0.38f, 1,  0.55f, 0.40f, 0.08f, 0.44f, 0,  0.10f, 0.35f, 0.20f, 0,  0.18f, 0.35f, 0.90f },
    { "Dreamscape",             0.08f, 0.55f, 2,  0.20f, 0.15f, 0.04f, 0.50f, 0,  0.20f, 0.50f, 0.40f, 1,  0.65f, 0.70f, 0.95f },
    { "Bit Rot",                0.20f, 0.70f, 0,  0.45f, 0.70f, 0.30f, 0.60f, 4,  0.35f, 0.70f, 0.60f, 2,  0.15f, 0.25f, 0.85f },
    { "Metallic",               0.05f, 0.60f, 0,  0.10f, 0.30f, 0.12f, 0.55f, 0,  0.50f, 0.65f, 0.55f, 0,  0.25f, 0.40f, 0.90f },
    { "Warped",                 0.18f, 0.50f, 1,  0.70f, 0.60f, 0.25f, 0.50f, 3,  0.20f, 0.40f, 0.35f, 0,  0.28f, 0.40f, 0.90f },
    { "Telephone",              0.25f, 0.65f, 0,  0.08f, 0.30f, 0.45f, 0.30f, 1,  0.35f, 0.55f, 0.40f, 2,  0.04f, 0.20f, 1.00f },
    { "Wrecked Radio",          0.55f, 0.45f, 0,  0.35f, 0.45f, 0.72f, 0.35f, 4,  0.25f, 0.40f, 0.30f, 0,  0.08f, 0.25f, 0.85f },
    { "Dark Matter",            0.30f, 0.30f, 2,  0.40f, 0.35f, 0.55f, 0.40f, 3,  0.40f, 0.30f, 0.45f, 1,  0.48f, 0.55f, 0.85f },
};

// ================================================================
// Parameter IDs (same order used for serialization)
// ================================================================

const char* const PresetManager::kParamIds[PresetManager::kNumParams] =
{
    "noise_amount", "noise_tone", "noise_type",
    "wobble_amount", "wobble_rate",
    "distort_amount", "distort_tone", "distort_type",
    "resonator_amount", "resonator_freq", "resonator_reso", "resonator_type",
    "space_amount", "space_decay",
    "mix"
};

// ================================================================

PresetManager::PresetManager() = default;

void PresetManager::setParam(juce::AudioProcessorValueTreeState& apvts,
                              const char* id, float value)
{
    if (auto* param = apvts.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

void PresetManager::applyPreset(const Preset& p,
                                 juce::AudioProcessorValueTreeState& apvts)
{
    // Noise
    setParam(apvts, "noise_amount",     p.noise);
    setParam(apvts, "noise_tone",       p.noiseTone);
    setParam(apvts, "noise_type",       static_cast<float>(p.noiseType));

    // Wobble
    setParam(apvts, "wobble_amount",    p.wobble);
    setParam(apvts, "wobble_rate",      p.wobbleRate);

    // Distort
    setParam(apvts, "distort_amount",   p.distort);
    setParam(apvts, "distort_tone",     p.distortTone);
    setParam(apvts, "distort_type",     static_cast<float>(p.distortType));

    // Resonator
    setParam(apvts, "resonator_amount", p.resonator);
    setParam(apvts, "resonator_freq",   p.resonatorFreq);
    setParam(apvts, "resonator_reso",   p.resonatorReso);
    setParam(apvts, "resonator_type",   static_cast<float>(p.resonatorType));

    // Space
    setParam(apvts, "space_amount",     p.space);
    setParam(apvts, "space_decay",      p.spaceDecay);

    // Master
    setParam(apvts, "mix",              p.mix);
}

void PresetManager::loadPreset(int index, juce::AudioProcessorValueTreeState& apvts)
{
    currentFactoryIndex = juce::jlimit(0, kNumPresets - 1, index);
    currentIsUser = false;
    currentUserName.clear();
    modified = false;
    applyPreset(kPresets[currentFactoryIndex], apvts);
}

void PresetManager::nextPreset(juce::AudioProcessorValueTreeState& apvts)
{
    loadPreset((currentFactoryIndex + 1) % kNumPresets, apvts);
}

void PresetManager::prevPreset(juce::AudioProcessorValueTreeState& apvts)
{
    loadPreset((currentFactoryIndex - 1 + kNumPresets) % kNumPresets, apvts);
}

void PresetManager::setCurrentIndexOnly(int index)
{
    currentFactoryIndex = juce::jlimit(0, kNumPresets - 1, index);
}

// ================================================================
// User preset directory
// ================================================================

juce::File PresetManager::getUserPresetsDir()
{
    auto appData = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory);

#if JUCE_MAC
    auto dir = appData.getChildFile("Futureproof Music School")
                      .getChildFile("Patina")
                      .getChildFile("Presets");
#else
    auto dir = appData.getChildFile("Futureproof Music School")
                      .getChildFile("Patina")
                      .getChildFile("Presets");
#endif

    if (!dir.exists())
        dir.createDirectory();

    return dir;
}

juce::File PresetManager::getUserPresetFile(const juce::String& name)
{
    return getUserPresetsDir().getChildFile(name + ".xml");
}

// ================================================================
// User preset list
// ================================================================

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    auto dir = getUserPresetsDir();

    if (dir.exists())
    {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*.xml");
        files.sort();

        for (auto& f : files)
            names.add(f.getFileNameWithoutExtension());
    }

    return names;
}

// ================================================================
// Save user preset
// ================================================================

bool PresetManager::saveUserPreset(const juce::String& name,
                                    juce::AudioProcessorValueTreeState& apvts)
{
    if (name.isEmpty())
        return false;

    auto file = getUserPresetFile(name);

    // Build XML from current parameter values
    auto xml = std::make_unique<juce::XmlElement>("PatinaPreset");
    xml->setAttribute("name", name);
    xml->setAttribute("version", "1.0");

    for (int i = 0; i < kNumParams; ++i)
    {
        if (auto* param = apvts.getParameter(kParamIds[i]))
        {
            auto* paramXml = xml->createNewChildElement("param");
            paramXml->setAttribute("id", kParamIds[i]);
            paramXml->setAttribute("value", static_cast<double>(param->getValue()));
        }
    }

    bool success = xml->writeTo(file);

    if (success)
    {
        currentIsUser = true;
        currentUserName = name;
        modified = false;
    }

    return success;
}

// ================================================================
// Load user preset
// ================================================================

bool PresetManager::loadUserPreset(const juce::String& name,
                                    juce::AudioProcessorValueTreeState& apvts)
{
    auto file = getUserPresetFile(name);

    if (!file.existsAsFile())
        return false;

    auto xml = juce::parseXML(file);

    if (xml == nullptr || !xml->hasTagName("PatinaPreset"))
        return false;

    // Apply each parameter
    for (auto* paramXml : xml->getChildIterator())
    {
        if (paramXml->hasTagName("param"))
        {
            auto id = paramXml->getStringAttribute("id");
            auto value = static_cast<float>(paramXml->getDoubleAttribute("value", 0.5));

            if (auto* param = apvts.getParameter(id))
                param->setValueNotifyingHost(value);
        }
    }

    currentIsUser = true;
    currentUserName = name;
    modified = false;

    return true;
}

// ================================================================
// Delete user preset
// ================================================================

bool PresetManager::deleteUserPreset(const juce::String& name)
{
    auto file = getUserPresetFile(name);

    if (!file.existsAsFile())
        return false;

    bool success = file.deleteFile();

    // If we deleted the current preset, go back to Init
    if (success && currentIsUser && currentUserName == name)
    {
        currentIsUser = false;
        currentUserName.clear();
        currentFactoryIndex = 0;
    }

    return success;
}

// ================================================================
// Global navigation (factory + user)
// ================================================================

int PresetManager::getTotalPresetCount() const
{
    return kNumPresets + getUserPresetNames().size();
}

juce::String PresetManager::getDisplayName() const
{
    if (currentIsUser)
        return currentUserName;

    return juce::String(kPresets[currentFactoryIndex].name);
}

const char* PresetManager::getCurrentName() const
{
    return kPresets[currentFactoryIndex].name;
}

void PresetManager::nextPresetGlobal(juce::AudioProcessorValueTreeState& apvts)
{
    auto userNames = getUserPresetNames();
    int totalUser = userNames.size();

    if (currentIsUser)
    {
        // Find current position in user list
        int idx = userNames.indexOf(currentUserName);

        if (idx >= 0 && idx < totalUser - 1)
        {
            // Next user preset
            loadUserPreset(userNames[idx + 1], apvts);
        }
        else
        {
            // Wrap to first factory preset
            loadPreset(0, apvts);
        }
    }
    else
    {
        if (currentFactoryIndex < kNumPresets - 1)
        {
            // Next factory preset
            loadPreset(currentFactoryIndex + 1, apvts);
        }
        else if (totalUser > 0)
        {
            // Jump to first user preset
            loadUserPreset(userNames[0], apvts);
        }
        else
        {
            // Wrap to first factory preset
            loadPreset(0, apvts);
        }
    }
}

void PresetManager::prevPresetGlobal(juce::AudioProcessorValueTreeState& apvts)
{
    auto userNames = getUserPresetNames();
    int totalUser = userNames.size();

    if (currentIsUser)
    {
        int idx = userNames.indexOf(currentUserName);

        if (idx > 0)
        {
            // Previous user preset
            loadUserPreset(userNames[idx - 1], apvts);
        }
        else
        {
            // Jump to last factory preset
            loadPreset(kNumPresets - 1, apvts);
        }
    }
    else
    {
        if (currentFactoryIndex > 0)
        {
            // Previous factory preset
            loadPreset(currentFactoryIndex - 1, apvts);
        }
        else if (totalUser > 0)
        {
            // Wrap to last user preset
            loadUserPreset(userNames[totalUser - 1], apvts);
        }
        else
        {
            // Wrap to last factory preset
            loadPreset(kNumPresets - 1, apvts);
        }
    }
}
