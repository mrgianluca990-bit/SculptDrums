#!/usr/bin/env python3
"""Apply Sculpt Drums internal-presets feature to the mixer build.

Usage: python3 apply_presets.py /path/to/SculptDrums
The script fails without altering sources if expected anchors do not match.
"""
from pathlib import Path
import shutil
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
source = root / "Source"
header = source / "PluginEditor.h"
cpp = source / "PluginEditor.cpp"
marker = "// SCULPT_DRUMS_INTERNAL_PRESETS_V1"

if not header.is_file() or not cpp.is_file() or not (source / "DrumChannels.h").is_file():
    raise SystemExit("Non trovo la struttura Source/PluginEditor.* e DrumChannels.h")

h = header.read_text(encoding="utf-8")
c = cpp.read_text(encoding="utf-8")
if marker in h or marker in c:
    raise SystemExit("Patch già applicata: nessun file modificato")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"Anchor {label}: atteso 1, trovato {count}. Sorgente diverso: stop senza modifiche.")
    return text.replace(old, new, 1)

h = replace_once(h, '#include "PluginProcessor.h"',
    '#include "PluginProcessor.h"\n#include "PresetStore.h"\n#include <functional>\n', "include")
h = replace_once(h,
    '    void updateMixerVisibility();',
    '''    void updateMixerVisibility();
    // SCULPT_DRUMS_INTERNAL_PRESETS_V1
    void presetError (const juce::String& message);
    void loadPresetList();
    void refreshPresetSelector (int selectedCustomIndex = -1);
    sculpt::DrumPreset capturePreset (const juce::String& name) const;
    void applyPreset (const sculpt::DrumPreset& preset);
    void selectPreset (int selectedId);
    void askPresetName (const juce::String& title, const juce::String& suggestion,
                        std::function<void (juce::String)> onAccept);
    int presetIndexNamed (const juce::String& name) const;
    bool persistPresets (const std::vector<sculpt::DrumPreset>& replacement);
    void savePresetAs (juce::String name);
    void saveCurrentPreset();
    void renameCurrentPreset();
    void deleteCurrentPreset();
    void showPresetOptions();''', "declarations")
h = replace_once(h, '    juce::TextButton mixerButton;',
    '''    juce::TextButton mixerButton;
    juce::ComboBox presetSelector;
    juce::TextButton presetSaveButton, presetOptionsButton;
    std::vector<sculpt::DrumPreset> presets;
    bool presetStoreHealthy = true;''', "members")

c = replace_once(c, '    addAndMakeVisible (mixerButton);',
    '''    addAndMakeVisible (mixerButton);

    presetSelector.setTooltip ("Preset interni — MORPH e volumi mixer");
    presetSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff242424));
    presetSelector.setColour (juce::ComboBox::textColourId, cream);
    presetSelector.setColour (juce::ComboBox::arrowColourId, cream);
    presetSelector.setColour (juce::ComboBox::outlineColourId, cream.withAlpha (0.22f));
    presetSelector.onChange = [this] { selectPreset (presetSelector.getSelectedId()); };
    addAndMakeVisible (presetSelector);

    presetSaveButton.setButtonText ("SAVE");
    presetSaveButton.setTooltip ("Salva i livelli attuali; se è un preset nuovo, chiedi il nome");
    presetSaveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
    presetSaveButton.setColour (juce::TextButton::textColourOffId, cream);
    presetSaveButton.onClick = [this] { saveCurrentPreset(); };
    addAndMakeVisible (presetSaveButton);

    presetOptionsButton.setButtonText ("...");
    presetOptionsButton.setTooltip ("Salva come, rinomina, elimina, ricarica");
    presetOptionsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
    presetOptionsButton.setColour (juce::TextButton::textColourOffId, cream);
    presetOptionsButton.onClick = [this] { showPresetOptions(); };
    addAndMakeVisible (presetOptionsButton);''', "constructor controls")
c = replace_once(c, '    setSize (768, 512);\n    startTimerHz (30);',
    '''    setSize (768, 512);
    loadPresetList();
    startTimerHz (30);''', "constructor load")
c = replace_once(c, '    mixerButton.setBounds (714, 12, 36, 28);',
    '''    mixerButton.setBounds (714, 12, 36, 28);
    presetSelector.setBounds (18, 12, 150, 28);
    presetSaveButton.setBounds (174, 12, 52, 28);
    presetOptionsButton.setBounds (231, 12, 32, 28);''', "resized")

c += '\n// SCULPT_DRUMS_INTERNAL_PRESETS_V1\n#include "PresetEditorMethods.inc"\n'

# All checks have now succeeded. Write backups and then the updated sources.
assets = Path(__file__).resolve().parent / "Source"
for filename in ("PresetStore.h", "PresetEditorMethods.inc"):
    if (source / filename).exists():
        raise SystemExit(f"Esiste già Source/{filename}: interrompo per evitare sovrascritture")
shutil.copy2(header, header.with_suffix(".h.pre-presets.bak"))
shutil.copy2(cpp, cpp.with_suffix(".cpp.pre-presets.bak"))
header.write_text(h, encoding="utf-8")
cpp.write_text(c, encoding="utf-8")
for filename in ("PresetStore.h", "PresetEditorMethods.inc"):
    shutil.copy2(assets / filename, source / filename)
print("Patch applicata: preset interni in Source/PresetStore.h e PluginEditor.*")
print("Backup .pre-presets.bak creati. Non aggiungerli al commit Git.")
