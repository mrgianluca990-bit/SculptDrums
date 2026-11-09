// === Sculpt MORPH Editor UI ===
void SculptDrumsEditor::rebuildMorphWavRows()
{
    wavRows.clear();
    wavRowContent.removeAllChildren();
    currentWavInfos = proc.getWavInfos();
    const int channel = juce::jlimit (0, (int) sculpt::mixerChannelCount - 1,
                                     pieceSelector.getSelectedId() - 1);
    const juce::String target = sculpt::mixerChannels[(std::size_t) channel].id;
    int row = 0;
    for (const auto& wav : currentWavInfos)
    {
        if (wav.pieceId != target) continue;
        auto component = std::make_unique<MorphWavRow> (proc, wav);
        wavRowContent.addAndMakeVisible (*component);
        component->setBounds (0, row * 30, 700, 30);
        wavRows.push_back (std::move (component));
        ++row;
    }
    wavRowContent.setSize (700, juce::jmax (30, row * 30));
    wavViewport.setViewPosition (0, 0);
    repaint();
}

void SculptDrumsEditor::refreshMorphEditorControls()
{
    const auto curve = proc.getMorphCurve();
    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        if (std::abs ((float) stagePositions[(std::size_t) i].getValue()
                       - curve.positions[(std::size_t) i]) > 0.09f)
            stagePositions[(std::size_t) i].setValue (curve.positions[(std::size_t) i],
                                                     juce::dontSendNotification);
        if (std::abs ((float) stageOverlaps[(std::size_t) i].getValue()
                       - curve.overlaps[(std::size_t) i]) > 0.09f)
            stageOverlaps[(std::size_t) i].setValue (curve.overlaps[(std::size_t) i],
                                                    juce::dontSendNotification);
    }
    for (auto& row : wavRows) row->syncFromProcessor();
    repaint();
}

void SculptDrumsEditor::drawMorphEditor (juce::Graphics& g)
{
    const auto area = juce::Rectangle<float> (12.0f, 68.0f, 744.0f, 432.0f);
    g.setColour (juce::Colour (0xff171717));
    g.fillRoundedRectangle (area, 11.0f);
    g.setColour (cream.withAlpha (0.13f));
    g.drawRoundedRectangle (area, 11.0f, 1.0f);

    const auto graph = juce::Rectangle<float> (22.0f, 112.0f, 718.0f, 112.0f);
    g.setColour (juce::Colour (0xff0d0d0d));
    g.fillRoundedRectangle (graph, 5.0f);
    const juce::Colour colours[6] = { warm, cream, amber, juce::Colour (0xffa9aa9d), cool, juce::Colour (0xff59dfd2) };
    const auto curve = proc.getMorphCurve();
    for (int stage = 0; stage < sculpt::stageCount; ++stage)
    {
        juce::Path path;
        for (int x = 0; x <= 200; ++x)
        {
            const float morphValue = x * 0.5f;
            const float w = sculpt::morphWeights (morphValue, 1.0f, curve).stage[(std::size_t) stage];
            const float px = graph.getX() + graph.getWidth() * morphValue / 100.0f;
            const float py = graph.getBottom() - 5.0f - (graph.getHeight() - 10.0f) * w;
            if (x == 0) path.startNewSubPath (px, py);
            else path.lineTo (px, py);
        }
        g.setColour (colours[stage].withAlpha (0.86f));
        g.strokePath (path, juce::PathStrokeType (1.8f));
        g.setColour (colours[stage].withAlpha (0.50f));
        const auto lineX = graph.getX() + graph.getWidth() * curve.positions[(std::size_t) stage] / 100.0f;
        g.drawVerticalLine ((int) lineX, graph.getY(), graph.getBottom());
    }
    const float currentMorph = proc.apvts.getRawParameterValue ("morph")->load();
    g.setColour (cream);
    g.drawVerticalLine ((int) (graph.getX() + graph.getWidth() * currentMorph / 100.0f),
                        graph.getY(), graph.getBottom());

    g.setColour (cream.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("POSIZIONE", 22, 232, 78, 12, juce::Justification::centredLeft);
    g.drawText ("OVERLAP", 22, 290, 78, 12, juce::Justification::centredLeft);
    g.drawText ("GAIN PER SINGOLO WAV  ·  doppio clic sul cursore = 0 dB",
                25, 347, 650, 16, juce::Justification::centredLeft);

    for (int i = 0; i < sculpt::stageCount; ++i)
    {
        g.setColour (colours[i]);
        const int x = 91 + i * 108;
        g.drawText (juce::String (i + 1), x, 232, 102, 12, juce::Justification::centred);
        g.drawText (juce::String (i + 1), x, 290, 102, 12, juce::Justification::centred);
    }
}
