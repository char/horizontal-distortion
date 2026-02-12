#pragma once

#include "CurveShapeEditor.h"
#include "CustomLAF.h"
#include "PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Slider::Listener,
                     private juce::Timer {
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void sliderValueChanged(juce::Slider* slider) override;

    void timerCallback() override;

    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    CustomLAF customLAF;

    juce::TextButton inspectButton { "Inspect" };
    juce::Slider depthSlider;
    juce::Label depthLabel;
    juce::Slider syncSlider;
    juce::Label syncLabel;
    juce::Slider dryWetSlider;
    juce::Label dryWetLabel;
    juce::Slider numeratorSlider;
    juce::Label ratioLabel;
    juce::Slider denominatorSlider;
    juce::Label ratioSeparatorLabel;
    juce::Label frequencyLabel;
    std::unique_ptr<CurveShapeEditor> curveShapeEditor;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> depthAttachment;
    std::unique_ptr<SliderAttachment> syncAttachment;
    std::unique_ptr<SliderAttachment> dryWetAttachment;
    std::unique_ptr<SliderAttachment> numeratorAttachment;
    std::unique_ptr<SliderAttachment> denominatorAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
