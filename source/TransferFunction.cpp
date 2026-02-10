#include "TransferFunction.h"
#include <algorithm>
#include <cmath>

TransferFunction::TransferFunction()
    : nodeBuffer(std::vector<juce::Point<float>> { { 0.0f, 0.0f }, { 1.0f, 1.0f } }) {
}

float TransferFunction::getRawValue(double phase) {
    const auto& nodes = nodeBuffer.read();
    if (nodes.size() < 2)
        return 0.5f;

    phase = std::fmod(phase, 1.0);
    if (phase < 0.0)
        phase += 1.0;

    size_t segmentIndex = 0;
    for (size_t j = 0; j < nodes.size() - 1; ++j) {
        if (phase >= nodes[j].x && phase <= nodes[j + 1].x) {
            segmentIndex = j;
            break;
        }
    }

    float x1 = nodes[segmentIndex].x;
    float y1 = nodes[segmentIndex].y;
    float x2 = nodes[segmentIndex + 1].x;
    float y2 = nodes[segmentIndex + 1].y;

    float t = x1 == x2 ? 0.0f : (float) ((phase - x1) / (x2 - x1));
    float value = y1 + (y2 - y1) * t;

    return juce::jlimit(0.0f, 1.0f, value);
}

float TransferFunction::getValue(double phase, float depth, float sync) {
    phase = std::fmod(phase, 1.0);
    if (phase < 0.0)
        phase += 1.0;

    auto syncPhase = std::fmod(phase * sync, 1.0);
    if (syncPhase < 0.0)
        syncPhase += 1.0;

    float rawValue = getRawValue(syncPhase);

    float result = std::lerp((float) phase, rawValue, depth);
    return juce::jlimit(0.0f, 1.0f, result);
}

void TransferFunction::setControlNodes(const std::vector<juce::Point<float>>& nodes) {
    if (!nodes.empty()) {
        auto& buffer = nodeBuffer.write();
        buffer = nodes;
        std::stable_sort(buffer.begin(), buffer.end(), [](const auto& a, const auto& b) { return a.x < b.x; });
        nodeBuffer.mark_dirty();
    }
}
