#include "PluginEditor.h"

namespace
{
const juce::Colour warm  { 0xffe8935a };
const juce::Colour amber { 0xfff0c060 };
const juce::Colour cool  { 0xff5fc8c0 };
const juce::Colour cream { 0xfff2ead2 };
constexpr float kA0 = juce::degreesToRadians (-135.0f);
constexpr float kA1 = juce::degreesToRadians ( 135.0f);
}

void SculptLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float knobR = r * 0.78f;

    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.setGradientFill (juce::ColourGradient (warm, b.getX(), c.y, cool, b.getRight(), c.y, false));
    g.strokePath (arc, juce::PathStrokeType (r > 60.0f ? 2.5f : 1.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0xff121212));
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (juce::Colour (0xff2c2c2c));
    g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f, 2.0f);

    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -knobR * 0.88f, 3.0f, knobR * 0.28f, 1.5f);
    g.setColour (cream);
    g.fillPath (tick, juce::AffineTransform::rotation (a0 + pos * (a1 - a0)).translated (c.x, c.y));
}

void SculptLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const float cx = (float) x + (float) w * 0.5f;
    const float top = (float) y + 8.0f;
    const float bottom = (float) (y + h) - 8.0f;
    g.setColour (juce::Colour (0xff090909));
    g.fillRoundedRectangle (cx - 3.5f, top, 7.0f, bottom - top, 3.0f);
    g.setColour (cream.withAlpha (0.17f));
    g.drawLine (cx, top, cx, bottom, 1.0f);
    g.setColour (warm.withAlpha (0.75f));
    g.drawLine (cx, sliderPos, cx, bottom, 2.5f);

    for (int i = 0; i <= 10; ++i)
    {
        const float yy = top + (bottom - top) * (float) i / 10.0f;
        g.setColour (cream.withAlpha (i == 2 ? 0.38f : 0.15f));
        g.drawLine (cx - 13.0f, yy, cx - 9.5f, yy, 1.0f);
        g.drawLine (cx + 9.5f, yy, cx + 13.0f, yy, 1.0f);
    }

    const float capW = juce::jmin (24.0f, (float) w - 4.0f);
    const auto cap = juce::Rectangle<float> (cx - capW * 0.5f, sliderPos - 10.0f, capW, 20.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b6254), cap.getX(), cap.getY(),
                                            juce::Colour (0xff242422), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (cream.withAlpha (0.8f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.drawLine (cx - capW * 0.34f, sliderPos, cx + capW * 0.34f, sliderPos, 1.5f);
}

SculptDrumsEditor::SculptDrumsEditor (SculptDrumsProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      morphAtt (p.apvts, "morph", morph), volumeAtt (p.apvts, "volume", volume)
{
    for (auto* s : { &morph, &volume })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (kA0, kA1, true);
        s->setLookAndFeel (&laf);
        addAndMakeVisible (*s);
    }
    morph.setDoubleClickReturnValue (true, 50.0);
    volume.setDoubleClickReturnValue (true, 0.0);

    mixerButton.setButtonText (juce::String::fromUTF8 ("≡"));
    mixerButton.setTooltip ("Mostra/nascondi il mixer dei singoli pezzi");
    mixerButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff242424));
    mixerButton.setColour (juce::TextButton::textColourOffId, cream);
    mixerButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff424037));
    mixerButton.setColour (juce::TextButton::textColourOnId, cream);
    mixerButton.setClickingTogglesState (true);
    mixerButton.onClick = [this]
    {
        mixerOpen = mixerButton.getToggleState();
        updateMixerVisibility();
        repaint();
    };
    addAndMakeVisible (mixerButton);

    for (std::size_t i = 0; i < sculpt::mixerChannelCount; ++i)
    {
        auto& fader = mixerFaders[i];
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        fader.setLookAndFeel (&laf);
        fader.setColour (juce::Slider::trackColourId, cream.withAlpha (0.35f));
        fader.setColour (juce::Slider::thumbColourId, warm);
        fader.setTextValueSuffix (" dB");
        fader.setPopupDisplayEnabled (true, true, this);
        fader.setDoubleClickReturnValue (true, 0.0);
        addChildComponent (fader);
        mixerAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            p.apvts, juce::String ("level_") + sculpt::mixerChannels[i].id, fader);
    }

    setSize (768, 512);
    startTimerHz (30);
}

SculptDrumsEditor::~SculptDrumsEditor()
{
    morph.setLookAndFeel (nullptr);
    volume.setLookAndFeel (nullptr);
    for (auto& fader : mixerFaders) fader.setLookAndFeel (nullptr);
}

void SculptDrumsEditor::resized()
{
    morph.setBounds (184, 50, 400, 400);
    volume.setBounds (48, 392, 84, 84);
    mixerButton.setBounds (714, 12, 36, 28);

    constexpr int left = 20;
    constexpr int mixerWidth = 728;
    for (std::size_t i = 0; i < sculpt::mixerChannelCount; ++i)
    {
        const int start = left + (int) (i * mixerWidth / sculpt::mixerChannelCount);
        const int end = left + (int) ((i + 1) * mixerWidth / sculpt::mixerChannelCount);
        mixerFaders[i].setBounds (start + 8, 148, end - start - 16, 253);
    }
    updateMixerVisibility();
}

void SculptDrumsEditor::updateMixerVisibility()
{
    morph.setVisible (! mixerOpen);
    volume.setVisible (! mixerOpen);
    for (auto& fader : mixerFaders) fader.setVisible (mixerOpen);
}

void SculptDrumsEditor::timerCallback()
{
    const float db = juce::Decibels::gainToDecibels (proc.meterLevel.load(), -60.0f);
    const float target = juce::jmap (juce::jlimit (-40.0f, 3.0f, db), -40.0f, 3.0f, 0.0f, 1.0f);
    needle += (target - needle) * 0.35f;
    repaint();
}

void SculptDrumsEditor::drawVU (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRoundedRectangle (r.expanded (4.0f), 8.0f);
    g.setColour (juce::Colour (0xfff0e6c8));
    g.fillRoundedRectangle (r, 5.0f);

    const juce::Point<float> pivot (r.getCentreX(), r.getBottom() - 4.0f);
    const float len = r.getHeight() * 0.78f;
    const float span = juce::degreesToRadians (48.0f);

    g.setColour (juce::Colour (0xff3a3328));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = -span + 2.0f * span * (float) i / 10.0f;
        const auto d = juce::Point<float> (std::sin (a), -std::cos (a));
        g.drawLine (juce::Line<float> (pivot + d * (len * 0.86f), pivot + d * len), i >= 8 ? 2.0f : 1.0f);
    }
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("VU", r.withTrimmedTop (r.getHeight() * 0.45f), juce::Justification::centred);

    const float a = -span + 2.0f * span * needle;
    g.setColour (juce::Colours::black);
    g.drawLine (juce::Line<float> (pivot, pivot + juce::Point<float> (std::sin (a), -std::cos (a)) * len * 0.95f), 1.6f);
    g.fillEllipse (pivot.x - 5.0f, pivot.y - 5.0f, 10.0f, 10.0f);
}

void SculptDrumsEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d0d));

    g.setColour (cream);
    g.setFont (juce::Font (juce::FontOptions (15.0f)));
    g.drawText ("S C U L P T   D R U M S", getLocalBounds().removeFromTop (44), juce::Justification::centred);

    g.setColour (cream.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (proc.getKitMessage(), 20, 44, getWidth() - 40, 16, juce::Justification::centred);

    if (mixerOpen)
    {
        const auto panel = juce::Rectangle<float> (12.0f, 72.0f, 744.0f, 426.0f);
        g.setColour (juce::Colour (0xff171717));
        g.fillRoundedRectangle (panel, 12.0f);
        g.setColour (cream.withAlpha (0.12f));
        g.drawRoundedRectangle (panel, 12.0f, 1.0f);

        constexpr int left = 20;
        constexpr int width = 728;
        for (std::size_t i = 0; i < sculpt::mixerChannelCount; ++i)
        {
            const int start = left + (int) (i * width / sculpt::mixerChannelCount);
            const int end = left + (int) ((i + 1) * width / sculpt::mixerChannelCount);
            const auto groupColour = i < 4 || (i >= 7 && i < 9) ? warm : i < 7 ? cool : amber;
            g.setColour (groupColour.withAlpha (0.7f));
            g.fillRoundedRectangle ((float) start + 11.0f, 112.0f,
                                    (float) (end - start - 22), 2.0f, 1.0f);
            g.setColour (cream.withAlpha (0.75f));
            g.setFont (juce::Font (juce::FontOptions (9.5f)));
            g.drawFittedText (sculpt::mixerChannels[i].shortName,
                              start, 417, end - start, 20, juce::Justification::centred, 1);
            g.setColour (cream.withAlpha (0.17f));
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (juce::String (sculpt::mixerChannels[i].midiNote),
                        start, 439, end - start, 14, juce::Justification::centred);
            if (i > 0) g.drawVerticalLine (start, 102.0f, 464.0f);
        }
        return;
    }

    // punti round robin
    const int step = proc.rrStep.load() % 5;
    for (int i = 0; i < 5; ++i)
    {
        const float cx = 384.0f + ((float) i - 2.0f) * 22.0f;
        const bool on = (i == step);
        g.setColour (cream.withAlpha (on ? 1.0f : 0.35f));
        g.fillEllipse (cx - (on ? 4.0f : 2.5f), 462.0f - (on ? 4.0f : 2.5f), on ? 8.0f : 5.0f, on ? 8.0f : 5.0f);
    }

    // tre famiglie: naturale / trattato / elettronico
    const juce::Colour cols[3] = { warm, amber, cool };
    for (int i = 0; i < 3; ++i)
    {
        const float w = proc.layerWeight[i].load();
        g.setColour (cols[i].withAlpha (0.25f + 0.75f * w));
        g.fillRoundedRectangle (294.0f + (float) i * 66.0f, 486.0f, 54.0f, 2.5f, 1.2f);
    }

    drawVU (g, { 590.0f, 408.0f, 150.0f, 80.0f });
}
