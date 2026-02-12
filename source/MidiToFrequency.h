#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

class MidiToFrequency {
public:
    MidiToFrequency() = default;

    void processMidiBuffer(const juce::MidiBuffer& midiMessages);

    std::optional<double> getCurrentFrequency() const;

    bool wasNoteOn() const { return noteOnOccurred; }

    void setPitchBendRange(double range) { pitchBendRange = range; }

    double getLastFrequency() const {
        if (lastMidiNote < 0)
            return -1.0;
        return midiNoteToFrequency(lastMidiNote + currentPitchBend);
    }

    void setLastFrequency(double frequency) {
        if (frequency < 0) {
            lastMidiNote = -1;
        } else {
            lastMidiNote = static_cast<int>(std::round(69.0 + 12.0 * std::log2(frequency / 440.0)));
            currentPitchBend = 0.0;
        }
    }

private:
    int lastMidiNote = -1;
    double currentPitchBend = 0.0;
    double pitchBendRange = 48.0;
    bool noteOnOccurred = false;

    double midiNoteToFrequency(double midiNote) const;
};

inline void MidiToFrequency::processMidiBuffer(const juce::MidiBuffer& midiMessages) {
    noteOnOccurred = false;

    for (auto event : midiMessages) {
        auto msg = event.getMessage();

        if (msg.isNoteOn()) {
            lastMidiNote = msg.getNoteNumber();
            currentPitchBend = 0.0;
            noteOnOccurred = true;
        } else if (msg.isPitchWheel()) {
            int pitchWheelValue = msg.getPitchWheelValue();
            currentPitchBend = ((pitchWheelValue - 8192) / 8192.0) * pitchBendRange;
        }
    }
}

inline std::optional<double> MidiToFrequency::getCurrentFrequency() const {
    if (lastMidiNote < 0) {
        return std::nullopt;
    }

    double midiNoteWithBend = lastMidiNote + currentPitchBend;
    return midiNoteToFrequency(midiNoteWithBend);
}

inline double MidiToFrequency::midiNoteToFrequency(double midiNote) const {
    return 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0);
}
