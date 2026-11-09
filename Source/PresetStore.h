#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>
#include <utility>
#include "DrumChannels.h"
#include "MorphPresetJson.h"

namespace sculpt
{
// Persistent, plugin-internal presets. These are deliberately separate from the
// state saved by the DAW. No WAV data, master volume or RR range is stored.
struct DrumPreset
{
    juce::String name;
    float morph = 50.0f;
    std::array<float, mixerChannelCount> levelsDb {};
    MorphEditorState editor;
};

struct PresetStore
{
    static juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Sculpt Drums").getChildFile ("presets.json");
    }

    static bool validName (const juce::String& original)
    {
        const auto name = original.trim();
        if (name.isEmpty() || name.length() > 48 || (name.equalsIgnoreCase ("Default") || name.equalsIgnoreCase ("Session"))
            || name == "." || name == "..")
            return false;
        for (auto c : name)
            if (c < 32 || c == '/' || c == '\\' || c == ':' || c == '"')
                return false;
        return true;
    }

    static bool read (std::vector<DrumPreset>& destination, juce::String& error)
    {
        destination.clear();
        error.clear();
        const auto path = file();
        if (! path.existsAsFile()) return true; // first launch: no custom presets
        const auto root = juce::JSON::parse (path);
        if (! root.isObject() || (static_cast<int> (root.getProperty ("version", 0)) != 1
                                   && static_cast<int> (root.getProperty ("version", 0)) != 2))
        {
            error = "Il file dei preset non contiene un JSON valido (versione 1 o 2): "
                      + path.getFullPathName();
            return false;
        }
        const auto all = root.getProperty ("presets", {});
        const auto* items = all.getArray();
        if (items == nullptr)
        {
            error = "Elenco dei preset non valido in " + path.getFullPathName();
            return false;
        }

        for (const auto& entry : *items)
        {
            if (! entry.isObject()) { error = "Preset danneggiato"; return false; }
            DrumPreset preset;
            preset.name = entry.getProperty ("name", {}).toString().trim();
            const auto values = entry.getProperty ("levelsDb", {});
            const auto* levels = values.getArray();
            const auto morphValue = entry.getProperty ("morph", {});
            if (! validName (preset.name) || levels == nullptr
                || levels->size() != static_cast<int> (mixerChannelCount)
                || ! (morphValue.isDouble() || morphValue.isInt() || morphValue.isInt64()))
            {
                error = "Dati incompleti o non validi nel preset '" + preset.name + "'";
                return false;
            }
            for (const auto& existing : destination)
                if (existing.name.equalsIgnoreCase (preset.name))
                {
                    error = "Nomi di preset duplicati: " + preset.name;
                    return false;
                }
            const auto m = static_cast<double> (morphValue);
            if (! std::isfinite (m) || m < 0.0 || m > 100.0)
            {
                error = "Valore MORPH non valido nel preset '" + preset.name + "'";
                return false;
            }
            preset.morph = static_cast<float> (m);
            for (std::size_t i = 0; i < mixerChannelCount; ++i)
            {
                const auto& value = (*levels)[static_cast<int> (i)];
                if (! (value.isDouble() || value.isInt() || value.isInt64()))
                {
                    error = "Livello del mixer non valido nel preset '" + preset.name + "'";
                    return false;
                }
                const auto db = static_cast<double> (value);
                if (! std::isfinite (db) || db < -72.0 || db > 12.0)
                {
                    error = "Livello fuori intervallo nel preset '" + preset.name + "'";
                    return false;
                }
                preset.levelsDb[i] = static_cast<float> (db);
            }
            if (! morphSettingsFromVar (entry.getProperty ("morphEditor", {}), preset.editor, error))
                return false;
            destination.push_back (std::move (preset));
        }
        return true;
    }

    static bool write (const std::vector<DrumPreset>& presets, juce::String& error)
    {
        error.clear();
        const auto path = file();
        const auto directory = path.getParentDirectory();
        if (! directory.isDirectory() && directory.createDirectory().failed())
        {
            error = "Impossibile creare la cartella: " + directory.getFullPathName();
            return false;
        }
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        root->setProperty ("version", 2);
        juce::Array<juce::var> items;
        for (const auto& preset : presets)
        {
            auto* item = new juce::DynamicObject();
            item->setProperty ("name", preset.name);
            item->setProperty ("morph", static_cast<double> (preset.morph));
            juce::Array<juce::var> values;
            for (auto level : preset.levelsDb)
                values.add (static_cast<double> (level));
            item->setProperty ("levelsDb", juce::var (values));
            item->setProperty ("morphEditor", morphSettingsToVar (preset.editor));
            items.add (juce::var (item));
        }
        root->setProperty ("presets", juce::var (items));
        const auto json = juce::JSON::toString (juce::var (root.get()), true);
        juce::TemporaryFile temporary (path);
        if (! temporary.getFile().replaceWithText (json)
            || ! temporary.overwriteTargetFileWithTemporary())
        {
            error = "Impossibile salvare i preset in " + path.getFullPathName();
            return false;
        }
        return true;
    }
};
} // namespace sculpt
