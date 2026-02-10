#pragma once

#include "Palette.h"
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class CurveShapeEditor;

//==============================================================================
class PhaseIndicatorOverlay : public juce::Component, private juce::Timer {
public:
    explicit PhaseIndicatorOverlay(PluginProcessor& processor)
        : processorRef(processor) {
        startTimerHz(30);
    }

    ~PhaseIndicatorOverlay() override {
        stopTimer();
    }

    void setCanvasArea(juce::Rectangle<int> area) {
        canvasArea = area;
    }

private:
    PluginProcessor& processorRef;
    juce::Rectangle<int> canvasArea;

    void paint(juce::Graphics& g) override {
        double phase = processorRef.getPhase();
        float y = processorRef.getCurveValue(phase);

        float localX = (float) phase * getWidth();
        float localY = getHeight() - y * getHeight();

        localY = juce::jlimit(0.0f, (float) getHeight(), localY);

        g.setColour(Palette::yellow.withAlpha(0.7f));
        g.drawVerticalLine((int) localX, 0.0f, (float) getHeight());

        const float discRadius = 8.0f;

        g.setColour(Palette::text);
        g.drawEllipse(localX - discRadius, localY - discRadius, discRadius * 2, discRadius * 2, 2.0f);

        g.setColour(Palette::yellow);
        g.fillEllipse(localX - discRadius, localY - discRadius, discRadius * 2, discRadius * 2);
    }

    bool hitTest(int x, int y) override {
        juce::ignoreUnused(x, y);
        return false;
    }

    void timerCallback() override {
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseIndicatorOverlay)
};

//==============================================================================
class CurveShapeEditor : public juce::Component,
                         private juce::Timer {
public:
    explicit CurveShapeEditor(PluginProcessor& processor);
    ~CurveShapeEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void modifierKeysChanged(const juce::ModifierKeys& modifiers) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;

private:
    PluginProcessor& processorRef;

    juce::TextButton resetButton { "Reset" };

    std::unique_ptr<PhaseIndicatorOverlay> phaseOverlay;

    juce::Rectangle<int> canvasArea;

    std::vector<juce::Point<float>> nodes;

    std::vector<std::vector<juce::Point<float>>> undoStack;
    std::vector<std::vector<juce::Point<float>>> redoStack;
    static constexpr int MAX_UNDO_STATES = 100;

    int selectedNodeIndex = -1;
    int hoveredNodeIndex = -1;
    bool isShiftHeld = false;
    static constexpr float NODE_HIT_RADIUS = 10.0f;

    juce::Point<float> dragStartPosition;
    bool isDragging = false;
    bool hasDragMoved = false;

    void syncNodesFromCurve();
    void syncNodesToCurve();
    void pushUndoState();
    void undo();
    void redo();
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    void drawWaveform(juce::Graphics& g);
    void drawGrid(juce::Graphics& g);
    void drawNodes(juce::Graphics& g);
    void drawNode(juce::Graphics& g, size_t index, juce::Point<float> screenPos, bool isSelected, bool isHovered);
    int findNodeAtPosition(juce::Point<float> screenPos);
    juce::Point<float> pointToScreen(float phaseX, float valueY);
    juce::Point<float> screenToPoint(const juce::Point<float>& screenPos);
    juce::Point<float> snapToGrid(juce::Point<float> pos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveShapeEditor)
};
