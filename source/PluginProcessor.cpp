#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
    #if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
    #endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      parameters(*this, nullptr, "Parameters", { std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "depth", 1 }, "Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 0.25f), 1.0f), std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "sync", 2 }, "Sync", juce::NormalisableRange<float>(1.0f, 32.0f, 0.0f), 1.0f), std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "dryWet", 3 }, "Dry/Wet", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f), 1.0f), std::make_unique<juce::AudioParameterInt>(juce::ParameterID { "numerator", 4 }, "Numerator", 1, 16, 1), std::make_unique<juce::AudioParameterInt>(juce::ParameterID { "denominator", 5 }, "Denominator", 1, 16, 1) }),
      oversampling(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false) {
}

PluginProcessor::~PluginProcessor() {
}

//==============================================================================
const juce::String PluginProcessor::getName() const {
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const {
    return true;
}

bool PluginProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int PluginProcessor::getNumPrograms() {
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram() {
    return 0;
}

void PluginProcessor::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
}

const juce::String PluginProcessor::getProgramName(int index) {
    juce::ignoreUnused(index);
    return {};
}

void PluginProcessor::changeProgramName(int index, const juce::String& newName) {
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    oversampling.initProcessing(samplesPerBlock);
    oversampling.reset();

    // its nice to have a big ring buffer it means u can play really low frequencies without artifacting
    int maxDelaySamples = static_cast<int>(std::ceil(oversampling.getOversamplingFactor() * 16384));

    ringBuffer.setSize(getTotalNumOutputChannels(), maxDelaySamples, false, true, false);
    ringBuffer.clear();
    writePosition = 0;

    int oversamplingLatency = static_cast<int>(oversampling.getLatencyInSamples());
    setLatencySamples(oversamplingLatency);

    currentDepth = parameters.getRawParameterValue("depth")->load();
    currentSync = parameters.getRawParameterValue("sync")->load();
    currentDryWet = parameters.getRawParameterValue("dryWet")->load();
}

void PluginProcessor::releaseResources() {
    ringBuffer.setSize(0, 0);
    writePosition = 0;
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    #if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    #endif

    return true;
#endif
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    midiToFreq.processMidiBuffer(midiMessages);
    if (midiToFreq.wasNoteOn()) {
        oscSamples = -midiMessages.getFirstEventTime();
    }
    double baseFreq = midiToFreq.getCurrentFrequency().value_or(1.0);

    // Apply numerator/denominator multiplier
    int numerator = parameters.getRawParameterValue("numerator")->load();
    int denominator = parameters.getRawParameterValue("denominator")->load();
    oscFreq = baseFreq * (static_cast<double>(numerator) / static_cast<double>(denominator));

    frequency.write() = oscFreq;
    frequency.mark_dirty();

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::AudioBlock<float> oversampledBlock = oversampling.processSamplesUp(block);
    int oversampledNumSamples = static_cast<int>(oversampledBlock.getNumSamples());

    float targetDepth = parameters.getRawParameterValue("depth")->load();
    float targetSync = parameters.getRawParameterValue("sync")->load();
    float targetDryWet = parameters.getRawParameterValue("dryWet")->load();

    float depthIncrement = (targetDepth - currentDepth) / oversampledNumSamples;
    float syncIncrement = (targetSync - currentSync) / oversampledNumSamples;
    float dryWetIncrement = (targetDryWet - currentDryWet) / oversampledNumSamples;

    double oversampledRate = getSampleRate() * oversampling.getOversamplingFactor();
    double oscPeriodSamples = oversampledRate / oscFreq;

    int ringBufferSize = ringBuffer.getNumSamples();

    if (ringBufferSize > 0 && ringBuffer.getNumChannels() > 0) {
        int requiredRingBufferSamples = (int) std::ceil(oscPeriodSamples * 2);
        if (ringBuffer.getNumSamples() < requiredRingBufferSamples) {
            // allocates but should be rare
            ringBuffer.setSize(getTotalNumOutputChannels(), requiredRingBufferSamples, true);
            ringBufferSize = requiredRingBufferSamples;
        }

        for (size_t channel = 0; channel < oversampledBlock.getNumChannels(); ++channel) {
            auto* channelData = oversampledBlock.getChannelPointer(channel);
            auto* ringData = ringBuffer.getWritePointer(static_cast<int>(channel));
            int localWritePos = writePosition;

            for (int sample = 0; sample < oversampledNumSamples; ++sample) {
                int64_t localSamples = oscSamples + sample;

                float drySample = channelData[sample];
                ringData[localWritePos] = drySample;

                auto localPhase = std::fmod(localSamples / oscPeriodSamples, 1.0);
                if (localPhase < 0.0)
                    localPhase += 1.0;

                // interpolate parameters per-sample
                float depthValue = currentDepth + depthIncrement * sample;
                float syncValue = currentSync + syncIncrement * sample;
                float dryWetValue = currentDryWet + dryWetIncrement * sample;

                // TODO: we should be able to calculate a block of transfer values ahead of time
                double lfoValue = (double) tf.getValue(localPhase, depthValue, syncValue);

                double readOffset = oscPeriodSamples * lfoValue - std::fmod((double) localSamples, oscPeriodSamples) - oscPeriodSamples;
                double readPos = localWritePos + readOffset;
                int readSampleIdxA = ((int) std::floor(readPos)) % ringBufferSize;
                int readSampleIdxB = ((int) std::ceil(readPos)) % ringBufferSize;
                if (readSampleIdxA < 0)
                    readSampleIdxA += ringBufferSize;
                if (readSampleIdxB < 0)
                    readSampleIdxB += ringBufferSize;
                // auto delayedSample = (float) std::lerp(ringData[readSampleIdxA], ringData[readSampleIdxB], std::fmod(readPos, 1.0));
                auto delayedSample = ringData[readSampleIdxA];
                channelData[sample] = drySample * (1.0f - dryWetValue) + delayedSample * dryWetValue;

                localWritePos++;
                localWritePos %= ringBufferSize;
            }
        }

        writePosition = (writePosition + oversampledNumSamples) % ringBufferSize;
        oscSamples += oversampledNumSamples;
    }

    phasor.write() = std::fmod(oscSamples / oscPeriodSamples, 1.0);
    phasor.mark_dirty();

    currentDepth = targetDepth;
    currentSync = targetSync;
    currentDryWet = targetDryWet;

    oversampling.processSamplesDown(block);
}

//==============================================================================
bool PluginProcessor::hasEditor() const {
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();

    // purge duplicate controlnodes/midistate entries - ideally we would be pruning before we copy to the parameters valuetree but whatever
    while (true) {
        int controlNodesIndex = state.indexOf(state.getChildWithName("ControlNodes"));
        if (controlNodesIndex < 0)
            break;
        state.removeChild(controlNodesIndex, nullptr);
    }

    while (true) {
        int midiStateIndex = state.indexOf(state.getChildWithName("MidiState"));
        if (midiStateIndex < 0)
            break;
        state.removeChild(midiStateIndex, nullptr);
    }

    juce::ValueTree nodesTree("ControlNodes");
    const auto& nodes = tf.nodes().read();

    for (const auto& node : nodes) {
        juce::ValueTree nodeTree("Node");
        nodeTree.setProperty("x", node.x, nullptr);
        nodeTree.setProperty("y", node.y, nullptr);
        nodesTree.appendChild(nodeTree, nullptr);
    }

    state.appendChild(nodesTree, nullptr);

    juce::ValueTree midiTree("MidiState");
    midiTree.setProperty("lastFrequency", midiToFreq.getLastFrequency(), nullptr);
    state.appendChild(midiTree, nullptr);

    auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (!xml)
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid())
        return;

    std::vector<juce::Point<float>> savedNodes;
    double lastFrequency = -1.0;

    auto nodesTree = state.getChildWithName("ControlNodes");
    if (nodesTree.isValid()) {
        for (int i = 0; i < nodesTree.getNumChildren(); ++i) {
            auto nodeTree = nodesTree.getChild(i);
            float x = nodeTree.getProperty("x", 0.0f);
            float y = nodeTree.getProperty("y", 0.5f);
            savedNodes.push_back({ x, y });
        }
    }

    auto midiTree = state.getChildWithName("MidiState");
    if (midiTree.isValid()) {
        lastFrequency = midiTree.getProperty("lastFrequency", -1.0);
    }

    parameters.replaceState(state);

    if (!savedNodes.empty()) {
        std::stable_sort(savedNodes.begin(), savedNodes.end(), [](const auto& a, const auto& b) { return a.x < b.x; });
        tf.nodes().setAll(savedNodes);
    }

    if (lastFrequency >= 0.0) {
        midiToFreq.setLastFrequency(lastFrequency);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PluginProcessor();
}
