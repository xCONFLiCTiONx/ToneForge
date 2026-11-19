#pragma once

#include <JuceHeader.h>

class ToneForgeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ToneForgeLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff191a1c));
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff0ba4f0));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2a2d36));
        setColour (juce::Slider::thumbColourId, juce::Colour (0xff0ba4f0));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff0f2f5));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff202228));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff343844));
        setColour (juce::Label::textColourId, juce::Colour (0xffe2e5eb));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff22252d));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff393d4b));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xfff0f2f5));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff0ba4f0));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff272a33));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff0ba4f0));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd0d4dd));
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff111215));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202228));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xfff0f2f5));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff0ba4f0));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xffffffff));

        // AlertWindow and Dialog styling
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff202228));
        setColour (juce::AlertWindow::textColourId, juce::Colour (0xfff0f2f5));
        setColour (juce::AlertWindow::outlineColourId, juce::Colour (0xff8049e2));

        // TextEditor styling
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff181a1e));
        setColour (juce::TextEditor::textColourId, juce::Colour (0xffffffff));
        setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff393d4b));
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff8049e2));
        setColour (juce::TextEditor::highlightColourId, juce::Colour (0x668049e2));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                              static_cast<float> (width), static_cast<float> (height)).reduced (2.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centre = bounds.getCentre();
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto lineW = juce::jlimit (2.5f, 5.0f, radius * 0.14f);
        auto arcRadius = radius - lineW * 0.6f;

        // Outer track background arc
        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff2a2d36));
        g.strokePath (backgroundArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active value arc with secondary electric blue gradient
        if (slider.isEnabled())
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
            g.strokePath (valueArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Inner knob dial body with brushed metal gradient
        float knobRadius = radius - lineW * 1.5f;
        if (knobRadius > 3.0f)
        {
            juce::ColourGradient knobGrad (juce::Colour (0xff323642), centre.x - knobRadius, centre.y - knobRadius,
                                          juce::Colour (0xff1c1e23), centre.x + knobRadius, centre.y + knobRadius, false);
            g.setGradientFill (knobGrad);
            g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

            // Knob border rim
            g.setColour (juce::Colour (0xff444957));
            g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.2f);

            // Central pointer indicator line
            juce::Path p;
            auto pointerLength = knobRadius * 0.75f;
            auto pointerThickness = juce::jmax (2.0f, knobRadius * 0.14f);
            p.addRoundedRectangle (-pointerThickness * 0.5f, -knobRadius + 2.0f, pointerThickness, pointerLength * 0.6f, 1.0f);
            p.applyTransform (juce::AffineTransform::rotation (toAngle).translated (centre.x, centre.y));

            g.setColour (slider.isEnabled() ? juce::Colour (0xff0ba4f0) : juce::Colour (0xff707480));
            g.fillPath (p);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                            bool /*shouldDrawButtonAsHighlighted*/,
                            bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = button.getLocalBounds();
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));

        const int boxSize = 15;
        int boxY = (bounds.getHeight() - boxSize) / 2;
        int boxX = 2;

        auto tickBounds = juce::Rectangle<float> ((float)boxX, (float)boxY, (float)boxSize, (float)boxSize);

        // Checkbox frame
        g.setColour (juce::Colour (0xff181a1e));
        g.fillRoundedRectangle (tickBounds, 3.0f);

        bool isMute = button.getButtonText().containsIgnoreCase ("MUTE");
        juce::Colour activeColour = isMute ? juce::Colour (0xffe63946) : juce::Colour (0xff0ba4f0);

        g.setColour (button.getToggleState() ? activeColour : juce::Colour (0xff424754));
        g.drawRoundedRectangle (tickBounds, 3.0f, button.getToggleState() ? 1.5f : 1.0f);

        // Active fill pip
        if (button.getToggleState())
        {
            g.setColour (activeColour);
            g.fillRoundedRectangle (tickBounds.reduced (3.0f), 2.0f);
        }

        // Button text
        g.setColour (button.getToggleState() ? juce::Colour (0xfff0f2f5) : juce::Colour (0xff828795));
        auto textArea = bounds.withTrimmedLeft (boxSize + 7);
        g.drawText (button.getButtonText(), textArea, juce::Justification::centredLeft, true);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        auto cornerSize = 4.0f;

        juce::Colour baseColour = backgroundColour;
        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker (0.25f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter (0.15f);

        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, cornerSize);

        g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xff8049e2) : juce::Colour (0xff3f4453));
        g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool /*shouldDrawButtonAsDown*/) override
    {
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xffffffff) : button.findColour (juce::TextButton::textColourOffId));
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float)width, (float)height).reduced (1.0f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        // Arrow
        auto arrowArea = juce::Rectangle<float> ((float)buttonX, (float)buttonY, (float)buttonW, (float)buttonH);
        juce::Path p;
        float arrowW = 8.0f;
        float arrowH = 5.0f;
        p.addTriangle (arrowArea.getCentreX() - arrowW * 0.5f, arrowArea.getCentreY() - arrowH * 0.5f,
                       arrowArea.getCentreX() + arrowW * 0.5f, arrowArea.getCentreY() - arrowH * 0.5f,
                       arrowArea.getCentreX(), arrowArea.getCentreY() + arrowH * 0.5f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (p);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (5, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    }
};

