#pragma once

#include "DoubleBuffer.h"
#include "MidiToFrequency.h"
#include "TransferFunction.h"
#include <algorithm>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

#if (MSVC)
    #include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor {
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

    float getGain() const { return parameters.getRawParameterValue("gain")->load(); }
    void setGain(float newGain) {
        if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("gain")))
            param->setValueNotifyingHost(parameters.getParameterRange("gain").convertTo0to1(juce::jlimit(0.0f, 2.0f, newGain)));
    }

    double getPhase() { return phasor.read(); }
    double getCurrentFrequency() { return frequency.read(); }

    float getCurveValue(float phase) {
        float depth = parameters.getRawParameterValue("depth")->load();
        float sync = parameters.getRawParameterValue("sync")->load();
        return tf.getValue(phase, depth, sync);
    }

    TransferFunction& getTF() { return tf; }
    const TransferFunction& getTF() const { return tf; }

private:
    static constexpr double WORST_CASE_LFO_FREQ = 8.176; // C-1 (MIDI note 0)

    juce::UndoManager undoManager;
    TransferFunction tf;
    DoubleBuffer<double> phasor { 0.0 };
    DoubleBuffer<double> frequency { 1.0 };

    double oscFreq = 1.0;
    // we store the phasor in integer samples so we can't lose precision
    int64_t oscSamples = 0;

    juce::AudioBuffer<float> ringBuffer;
    int writePosition = 0;

    MidiToFrequency midiToFreq;

    juce::dsp::Oversampling<float> oversampling;

    float currentDepth = 0.0f;
    float currentSync = 1.0f;
    float currentDryWet = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
