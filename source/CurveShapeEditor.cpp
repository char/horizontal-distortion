#include "CurveShapeEditor.h"
#include "Palette.h"
#include <algorithm>
#include <cmath>

CurveShapeEditor::CurveShapeEditor(PluginProcessor& processor)
    : processorRef(processor) {
    addAndMakeVisible(resetButton);
    resetButton.onClick = [this] {
        pushUndoState();
        nodes = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
        syncNodesToCurve();
        repaint();
    };

    phaseOverlay = std::make_unique<PhaseIndicatorOverlay>(processor);
    addAndMakeVisible(*phaseOverlay);

    addMouseListener(this, false);
    setWantsKeyboardFocus(true);
    syncNodesFromCurve();

    startTimerHz(30);
}

CurveShapeEditor::~CurveShapeEditor() {
    stopTimer();
}

void CurveShapeEditor::paint(juce::Graphics& g) {
    g.fillAll(Palette::base);

    g.setColour(Palette::surface0);
    g.fillRect(canvasArea);
    g.setColour(Palette::overlay0);
    g.drawRect(canvasArea, 2);

    if (isShiftHeld)
        drawGrid(g);

    drawWaveform(g);
    drawNodes(g);

    g.setColour(Palette::text);
    g.setFont(14.0f);
    g.drawText("Curve Shape Editor - Click to add, drag to edit, right-click to remove, Shift to snap",
        getLocalBounds().removeFromTop(25),
        juce::Justification::centred);
}

void CurveShapeEditor::resized() {
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(25);
    canvasArea = area.removeFromTop(area.getHeight() - 50);

    phaseOverlay->setCanvasArea(canvasArea);
    phaseOverlay->setBounds(canvasArea);

    resetButton.setBounds(area.withTrimmedTop(10).removeFromLeft(area.getWidth() / 6 - 5));
}

void CurveShapeEditor::drawWaveform(juce::Graphics& g) {
    if (nodes.size() < 2)
        return;

    std::vector<float> xPositions;

    const int numSamples = 256;
    for (int i = 0; i <= numSamples; ++i) {
        xPositions.push_back(i / (float) numSamples);
    }

    for (const auto& node : nodes) {
        xPositions.push_back(node.x);
    }

    std::sort(xPositions.begin(), xPositions.end());
    xPositions.erase(std::unique(xPositions.begin(), xPositions.end()), xPositions.end());

    juce::Path path;
    for (size_t i = 0; i < xPositions.size(); ++i) {
        float phase = xPositions[i];
        float value = processorRef.getCurveValue(phase);
        auto point = pointToScreen(phase, value);

        if (i == 0)
            path.startNewSubPath(point);
        else
            path.lineTo(point);
    }

    g.setColour(Palette::pink);
    g.strokePath(path, juce::PathStrokeType(2.5f));
}

void CurveShapeEditor::drawGrid(juce::Graphics& g) {
    const int gridDivisions = 32;

    g.setColour(Palette::overlay0.withAlpha(0.3f));

    for (int i = 1; i < gridDivisions; ++i) {
        float x = canvasArea.getX() + (canvasArea.getWidth() / (float) gridDivisions) * i;
        g.setColour(Palette::overlay0.withAlpha((i % 8 == 0) ? 0.5f : 0.3f));
        g.drawVerticalLine((int) x, (float) canvasArea.getY(), (float) canvasArea.getBottom());
    }

    for (int i = 1; i < gridDivisions; ++i) {
        float y = canvasArea.getY() + (canvasArea.getHeight() / (float) gridDivisions) * i;
        g.setColour(Palette::overlay0.withAlpha((i % 8 == 0) ? 0.5f : 0.3f));
        g.drawHorizontalLine((int) y, (float) canvasArea.getX(), (float) canvasArea.getRight());
    }

    g.setColour(Palette::overlay0.withAlpha(0.7f));
    float centerY = canvasArea.getY() + canvasArea.getHeight() / 2.0f;
    g.drawHorizontalLine((int) centerY, (float) canvasArea.getX(), (float) canvasArea.getRight());
}

void CurveShapeEditor::mouseDown(const juce::MouseEvent& event) {
    grabKeyboardFocus();

    selectedNodeIndex = findNodeAtPosition(event.position.toFloat());

    if (selectedNodeIndex == -1 && !event.mods.isRightButtonDown()) {
        auto clickPos = event.position.toFloat();
        auto pos = screenToPoint(clickPos);
        pos.x = juce::jlimit(0.0f, 1.0f, pos.x);
        pos.y = juce::jlimit(0.0f, 1.0f, pos.y);

        if (event.mods.isShiftDown())
            pos = snapToGrid(pos);

        pushUndoState();

        nodes.push_back(pos);
        syncNodesToCurve();

        selectedNodeIndex = (int) nodes.size() - 1;
        dragStartPosition = pos;
        isDragging = true;
        hasDragMoved = false;
        repaint();
    } else if (selectedNodeIndex != -1) {
        if (selectedNodeIndex >= 0 && selectedNodeIndex < (int) nodes.size()) {
            dragStartPosition = nodes[selectedNodeIndex];
            isDragging = true;
            hasDragMoved = false;
        }
    }
}

void CurveShapeEditor::mouseDrag(const juce::MouseEvent& event) {
    if (selectedNodeIndex == -1 || selectedNodeIndex >= (int) nodes.size())
        return;

    auto pos = screenToPoint(event.position.toFloat());
    pos.x = juce::jlimit(0.0f, 1.0f, pos.x);
    pos.y = juce::jlimit(0.0f, 1.0f, pos.y);

    if (event.mods.isShiftDown())
        pos = snapToGrid(pos);

    const float movementThreshold = 0.001f;
    if (!hasDragMoved) {
        if (std::abs(pos.x - dragStartPosition.x) > movementThreshold || std::abs(pos.y - dragStartPosition.y) > movementThreshold) {
            pushUndoState();
            hasDragMoved = true;
        }
    }

    nodes[selectedNodeIndex] = pos;
    syncNodesToCurve();
    repaint();
}

void CurveShapeEditor::mouseUp(const juce::MouseEvent& event) {
    if (event.mods.isRightButtonDown() && selectedNodeIndex != -1) {
        if (nodes.size() > 2) {
            pushUndoState();
            nodes.erase(nodes.begin() + selectedNodeIndex);
            syncNodesToCurve();
            repaint();
        }
    }

    isDragging = false;
    hasDragMoved = false;
    selectedNodeIndex = -1;
}

void CurveShapeEditor::mouseMove(const juce::MouseEvent& event) {
    int newHovered = findNodeAtPosition(event.position.toFloat());
    if (newHovered != hoveredNodeIndex) {
        hoveredNodeIndex = newHovered;
        repaint();
    }
}

void CurveShapeEditor::modifierKeysChanged(const juce::ModifierKeys& modifiers) {
    bool newShiftState = modifiers.isShiftDown();
    if (newShiftState != isShiftHeld) {
        isShiftHeld = newShiftState;
        repaint();
    }
}

bool CurveShapeEditor::keyPressed(const juce::KeyPress& key) {
    if (key.isKeyCode('Z')) {
        auto modifiers = key.getModifiers();

        if (modifiers.isCommandDown()) {
            if (modifiers.isShiftDown()) {
                if (canRedo()) {
                    redo();
                    repaint();
                    return true;
                }
            } else {
                if (canUndo()) {
                    undo();
                    repaint();
                    return true;
                }
            }
        }
    }

    return Component::keyPressed(key);
}

int CurveShapeEditor::findNodeAtPosition(juce::Point<float> screenPos) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto nodeScreenPos = pointToScreen(nodes[i].x, nodes[i].y);
        if (nodeScreenPos.getDistanceFrom(screenPos) <= NODE_HIT_RADIUS)
            return (int) i;
    }

    return -1;
}

void CurveShapeEditor::drawNodes(juce::Graphics& g) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto screenPos = pointToScreen(nodes[i].x, nodes[i].y);
        bool isSelected = (i == (size_t) selectedNodeIndex);
        bool isHovered = (i == (size_t) hoveredNodeIndex);

        drawNode(g, i, screenPos, isSelected, isHovered);
    }

    g.setColour(Palette::text);
    g.setFont(12.0f);
    g.drawText(juce::String(nodes.size()) + " nodes", canvasArea.getX() + 5, canvasArea.getY() + 5, 100, 20, juce::Justification::left, false);
}

void CurveShapeEditor::drawNode(juce::Graphics& g, size_t index, juce::Point<float> pos, bool isSelected, bool isHovered) {
    juce::ignoreUnused(index);

    float radius = isSelected ? 6.0f : 5.0f;

    g.setColour(Palette::text);
    g.drawEllipse(pos.x - radius, pos.y - radius, radius * 2, radius * 2, 2.0f);

    if (isSelected)
        g.setColour(Palette::yellow);
    else if (isHovered)
        g.setColour(Palette::peach);
    else
        g.setColour(Palette::sky);

    g.fillEllipse(pos.x - radius, pos.y - radius, radius * 2, radius * 2);
}

juce::Point<float> CurveShapeEditor::pointToScreen(float phaseX, float valueY) {
    float x = canvasArea.getX() + phaseX * canvasArea.getWidth();
    float y = canvasArea.getBottom() - valueY * canvasArea.getHeight();
    return { x, y };
}

juce::Point<float> CurveShapeEditor::screenToPoint(const juce::Point<float>& screenPos) {
    float phaseX = (screenPos.x - canvasArea.getX()) / canvasArea.getWidth();
    float valueY = (canvasArea.getBottom() - screenPos.y) / canvasArea.getHeight();
    return { phaseX, valueY };
}

juce::Point<float> CurveShapeEditor::snapToGrid(juce::Point<float> pos) {
    const float gridSize = 1.0f / 32.0f;

    float snappedX = std::round(pos.x / gridSize) * gridSize;
    float snappedY = std::round(pos.y / gridSize) * gridSize;
    snappedX = juce::jlimit(0.0f, 1.0f, snappedX);
    snappedY = juce::jlimit(0.0f, 1.0f, snappedY);

    return { snappedX, snappedY };
}

void CurveShapeEditor::syncNodesFromCurve() {
    nodes = processorRef.getTF().nodes().read();
}

void CurveShapeEditor::syncNodesToCurve() {
    processorRef.getTF().setControlNodes(nodes);
}

void CurveShapeEditor::pushUndoState() {
    undoStack.push_back(nodes);
    redoStack.clear();

    if (undoStack.size() > MAX_UNDO_STATES)
        undoStack.erase(undoStack.begin());
}

void CurveShapeEditor::undo() {
    if (undoStack.empty())
        return;

    redoStack.push_back(nodes);

    auto previousState = undoStack.back();
    undoStack.pop_back();

    nodes = previousState;
    syncNodesToCurve();
}

void CurveShapeEditor::redo() {
    if (redoStack.empty())
        return;

    undoStack.push_back(nodes);

    auto nextState = redoStack.back();
    redoStack.pop_back();

    nodes = nextState;
    syncNodesToCurve();
}

void CurveShapeEditor::timerCallback() {
    repaint();
}
