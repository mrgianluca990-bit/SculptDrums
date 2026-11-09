// Included at the end of PluginEditor.cpp. UI operations are performed on the
// message thread only; the APVTS already smooths MORPH and channel gains.

void SculptDrumsEditor::presetError (const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                            "Sculpt Drums — Presets", message, "OK", this);
}

void SculptDrumsEditor::refreshPresetSelector (int selectedCustomIndex)
{
    presetSelector.clear (juce::dontSendNotification);
    presetSelector.addItem ("Session", 1); // current DAW state, not a preset
    presetSelector.addItem ("Default", 2);
    for (std::size_t i = 0; i < presets.size(); ++i)
        presetSelector.addItem (presets[i].name, static_cast<int> (i) + 3);
    const int selectId = selectedCustomIndex >= 0 ? selectedCustomIndex + 3 : 1;
    presetSelector.setSelectedId (selectId, juce::dontSendNotification);
}

void SculptDrumsEditor::loadPresetList()
{
    juce::String error;
    presetStoreHealthy = sculpt::PresetStore::read (presets, error);
    refreshPresetSelector();
    if (! presetStoreHealthy)
    {
        // Preserve the damaged file: refuse writes until it is repaired.
        presetSaveButton.setEnabled (false);
        presetOptionsButton.setEnabled (false);
        presetError (error + "\nIl file originale non viene sovrascritto.");
    }
}

sculpt::DrumPreset SculptDrumsEditor::capturePreset (const juce::String& name) const
{
    sculpt::DrumPreset preset;
    preset.name = name;
    preset.morph = proc.apvts.getRawParameterValue ("morph")->load();
    for (std::size_t i = 0; i < sculpt::mixerChannelCount; ++i)
        preset.levelsDb[i] = proc.apvts.getRawParameterValue (
            juce::String ("level_") + sculpt::mixerChannels[i].id)->load();
    return preset;
}

void SculptDrumsEditor::applyPreset (const sculpt::DrumPreset& preset)
{
    const auto setParameter = [this] (const juce::String& parameterId, float value)
    {
        if (auto* param = proc.apvts.getParameter (parameterId))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };
    setParameter ("morph", preset.morph);
    for (std::size_t i = 0; i < sculpt::mixerChannelCount; ++i)
        setParameter (juce::String ("level_") + sculpt::mixerChannels[i].id,
                      preset.levelsDb[i]);
    // Master volume and RR Range are intentionally NOT modified.
}

void SculptDrumsEditor::selectPreset (int selectedId)
{
    if (selectedId == 2)
    {
        applyPreset (sculpt::DrumPreset{});
        return;
    }
    const int index = selectedId - 3;
    if (index >= 0 && index < static_cast<int> (presets.size()))
        applyPreset (presets[static_cast<std::size_t> (index)]);
}

void SculptDrumsEditor::askPresetName (const juce::String& title,
                                       const juce::String& suggestion,
                                       std::function<void (juce::String)> onAccept)
{
    auto* dialog = new juce::AlertWindow (title, "Nome del preset (max 48 caratteri)",
                                         juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor ("name", suggestion, "Nome");
    dialog->addButton ("Conferma", 1);
    dialog->addButton ("Annulla", 0);
    dialog->enterModalState (true,
        juce::ModalCallbackFunction::create ([dialog, callback = std::move (onAccept)] (int code) mutable
        {
            const auto newName = dialog->getTextEditorContents ("name").trim();
            delete dialog;
            if (code == 1) callback (newName);
        }), false);
}

int SculptDrumsEditor::presetIndexNamed (const juce::String& name) const
{
    for (int i = 0; i < static_cast<int> (presets.size()); ++i)
        if (presets[static_cast<std::size_t> (i)].name.equalsIgnoreCase (name)) return i;
    return -1;
}

bool SculptDrumsEditor::persistPresets (const std::vector<sculpt::DrumPreset>& replacement)
{
    juce::String error;
    if (! sculpt::PresetStore::write (replacement, error))
    {
        presetError (error);
        return false;
    }
    presets = replacement;
    return true;
}

void SculptDrumsEditor::savePresetAs (juce::String name)
{
    name = name.trim();
    if (! sculpt::PresetStore::validName (name))
    {
        presetError ("Usa un nome da 1 a 48 caratteri, senza / \\ : o virgolette. 'Default' e 'Session' sono riservati.");
        return;
    }
    if (presetIndexNamed (name) >= 0)
    {
        presetError ("Esiste già un preset con questo nome. Selezionalo e premi Salva per aggiornarlo.");
        return;
    }
    auto updated = presets;
    updated.push_back (capturePreset (name));
    if (! persistPresets (updated)) return;
    refreshPresetSelector (static_cast<int> (presets.size()) - 1);
}

void SculptDrumsEditor::saveCurrentPreset()
{
    if (! presetStoreHealthy) return;
    const int index = presetSelector.getSelectedId() - 3;
    if (index >= 0 && index < static_cast<int> (presets.size()))
    {
        auto updated = presets;
        updated[static_cast<std::size_t> (index)] = capturePreset (updated[static_cast<std::size_t> (index)].name);
        if (persistPresets (updated)) refreshPresetSelector (index);
        return;
    }
    auto safe = juce::Component::SafePointer<SculptDrumsEditor> (this);
    askPresetName ("Salva preset", "Nuovo preset", [safe] (juce::String name)
    {
        if (safe != nullptr) safe->savePresetAs (name);
    });
}

void SculptDrumsEditor::renameCurrentPreset()
{
    if (! presetStoreHealthy) return;
    const int index = presetSelector.getSelectedId() - 3;
    if (index < 0 || index >= static_cast<int> (presets.size())) return;
    const auto oldName = presets[static_cast<std::size_t> (index)].name;
    auto safe = juce::Component::SafePointer<SculptDrumsEditor> (this);
    askPresetName ("Rinomina preset", oldName, [safe, oldName] (juce::String newName)
    {
        if (safe == nullptr) return;
        newName = newName.trim();
        if (! sculpt::PresetStore::validName (newName))
        {
            safe->presetError ("Nome non valido o riservato.");
            return;
        }
        const int currentIndex = safe->presetIndexNamed (oldName);
        if (currentIndex < 0) return;
        const int duplicate = safe->presetIndexNamed (newName);
        if (duplicate >= 0 && duplicate != currentIndex)
        {
            safe->presetError ("Un altro preset ha già questo nome.");
            return;
        }
        auto updated = safe->presets;
        updated[static_cast<std::size_t> (currentIndex)].name = newName;
        if (safe->persistPresets (updated)) safe->refreshPresetSelector (currentIndex);
    });
}

void SculptDrumsEditor::deleteCurrentPreset()
{
    if (! presetStoreHealthy) return;
    const int index = presetSelector.getSelectedId() - 3;
    if (index < 0 || index >= static_cast<int> (presets.size())) return;
    const auto name = presets[static_cast<std::size_t> (index)].name;
    auto safe = juce::Component::SafePointer<SculptDrumsEditor> (this);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
        "Elimina preset", "Eliminare definitivamente '" + name + "'?", "Elimina", "Annulla", this,
        juce::ModalCallbackFunction::create ([safe, name] (int result)
        {
            if (safe == nullptr || result != 1) return;
            const int actualIndex = safe->presetIndexNamed (name);
            if (actualIndex < 0) return;
            auto updated = safe->presets;
            updated.erase (updated.begin() + actualIndex);
            if (safe->persistPresets (updated)) safe->refreshPresetSelector();
        }));
}

void SculptDrumsEditor::showPresetOptions()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Salva come...");
    const bool hasNamedPreset = presetSelector.getSelectedId() >= 3;
    menu.addItem (2, "Rinomina", hasNamedPreset);
    menu.addItem (3, "Elimina", hasNamedPreset);
    menu.addSeparator();
    menu.addItem (4, "Ricarica elenco");
    auto safe = juce::Component::SafePointer<SculptDrumsEditor> (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetOptionsButton),
                        [safe] (int selection)
    {
        if (safe == nullptr) return;
        switch (selection)
        {
            case 1:
            {
                auto s = safe;
                safe->askPresetName ("Salva come", "Nuovo preset", [s] (juce::String name)
                {
                    if (s != nullptr) s->savePresetAs (name);
                });
                break;
            }
            case 2: safe->renameCurrentPreset(); break;
            case 3: safe->deleteCurrentPreset(); break;
            case 4: safe->loadPresetList(); break;
            default: break;
        }
    });
}
