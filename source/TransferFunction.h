#pragma once

#include "DoubleBuffer.h"
#include <algorithm>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class TransferFunction {
public:
    TransferFunction();

    // audio thread ideally
    float getValue(double phase, float depth, float sync = 1.0f);
    float getRawValue(double phase);

    // ui thread only
    void setControlNodes(const std::vector<juce::Point<float>>& nodes);

    DoubleBuffer<std::vector<juce::Point<float>>>& nodes() {
        return nodeBuffer;
    }

private:
    DoubleBuffer<std::vector<juce::Point<float>>> nodeBuffer;
};
