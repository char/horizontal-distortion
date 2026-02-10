#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"

class CustomLAF : public juce::LookAndFeel_V4 {
public:
    CustomLAF() {
        setColour(juce::Slider::thumbColourId, Palette::sapphire);
        setColour(juce::Slider::trackColourId, Palette::surface1);
        setColour(juce::Slider::backgroundColourId, Palette::surface0);
        setColour(juce::Slider::textBoxOutlineColourId, Palette::overlay0);
        
        setColour(juce::TextButton::buttonColourId, Palette::surface1);
        setColour(juce::TextButton::textColourOffId, Palette::text);
        setColour(juce::TextButton::textColourOnId, Palette::text);
        setColour(juce::ComboBox::backgroundColourId, Palette::surface1);
        setColour(juce::ComboBox::textColourId, Palette::text);
        setColour(juce::ComboBox::outlineColourId, Palette::overlay0);
        
        setColour(juce::Label::textColourId, Palette::text);
    }
};
