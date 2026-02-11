#include "PluginEditor.h"
#include "Palette.h"

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), depthAttachment(std::make_unique<SliderAttachment>(p.parameters, "depth", depthSlider)), syncAttachment(std::make_unique<SliderAttachment>(p.parameters, "sync", syncSlider)), dryWetAttachment(std::make_unique<SliderAttachment>(p.parameters, "dryWet", dryWetSlider)) {
    juce::ignoreUnused(processorRef);

    setLookAndFeel(&customLAF);

    depthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    depthSlider.setRange(0.0, 1.0, 0.0);
    addAndMakeVisible(depthSlider);

    depthLabel.setText("Depth", juce::dontSendNotification);
    depthLabel.attachToComponent(&depthSlider, true);
    addAndMakeVisible(depthLabel);

    syncSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    syncSlider.setRange(1.0, 32.0, 0.0);
    addAndMakeVisible(syncSlider);

    syncLabel.setText("Sync", juce::dontSendNotification);
    syncLabel.attachToComponent(&syncSlider, true);
    addAndMakeVisible(syncLabel);

    dryWetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    dryWetSlider.setRange(0.0, 1.0, 0.0);
    addAndMakeVisible(dryWetSlider);

    dryWetLabel.setText("Dry/Wet", juce::dontSendNotification);
    dryWetLabel.attachToComponent(&dryWetSlider, true);
    addAndMakeVisible(dryWetLabel);

    frequencyLabel.setJustificationType(juce::Justification::centred);
    frequencyLabel.setColour(juce::Label::textColourId, Palette::text);
    frequencyLabel.setFont(juce::Font(14.0f));
    addAndMakeVisible(frequencyLabel);

    startTimer(100);

    curveShapeEditor = std::make_unique<CurveShapeEditor>(processorRef);
    addAndMakeVisible(*curveShapeEditor);

    addAndMakeVisible(inspectButton);
    inspectButton.onClick = [&] {
        if (!inspector) {
            inspector = std::make_unique<melatonin::Inspector>(*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible(true);
    };

    setSize(700, 600);
}

PluginEditor::~PluginEditor() {
    stopTimer();
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(Palette::base);

    auto area = getLocalBounds();
    g.setColour(Palette::text);
    g.setFont(16.0f);
    g.drawText("Horizontal Distortion", area.removeFromTop(40), juce::Justification::centred, false);
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(10);

    area.removeFromTop(40);

    auto controlArea = area.removeFromTop(150);

    auto depthArea = controlArea.removeFromTop(50);
    depthLabel.setBounds(depthArea.removeFromLeft(80));
    depthSlider.setBounds(depthArea);

    auto syncArea = controlArea.removeFromTop(50);
    syncLabel.setBounds(syncArea.removeFromLeft(80));
    syncSlider.setBounds(syncArea);

    auto dryWetArea = controlArea.removeFromTop(50);
    dryWetLabel.setBounds(dryWetArea.removeFromLeft(80));
    dryWetSlider.setBounds(dryWetArea);

    frequencyLabel.setBounds(area.removeFromTop(30));

    if (curveShapeEditor) {
        auto editorArea = area.removeFromTop(area.getHeight() - 40);
        curveShapeEditor->setBounds(editorArea);
    }

    inspectButton.setBounds(area.withSizeKeepingCentre(100, 40));
}

void PluginEditor::sliderValueChanged(juce::Slider* slider) {
}

void PluginEditor::timerCallback() {
    double freq = processorRef.getCurrentFrequency();

    juce::String freqText;
    if (freq >= 1000.0) {
        freqText = juce::String(freq / 1000.0, 2) + " kHz";
    } else {
        freqText = juce::String(freq, 1) + " Hz";
    }

    frequencyLabel.setText(freqText, juce::dontSendNotification);
}
