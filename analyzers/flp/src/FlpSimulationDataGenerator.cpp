#include "FlpSimulationDataGenerator.h"

#include "FlpAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

FlpSimulationDataGenerator::FlpSimulationDataGenerator() : mSettings(0), mSimulationSampleRateHz(0), mBurstsSent(0) {
}

FlpSimulationDataGenerator::~FlpSimulationDataGenerator() {
}

void FlpSimulationDataGenerator::Initialize(U32 simulation_sample_rate, FlpAnalyzerSettings* settings) {
    mSimulationSampleRateHz = simulation_sample_rate;
    mSettings = settings;
    mBurstsSent = 0;

    // Idle is the opposite of the pulse level.
    const BitState idle = (mSettings->mPolarity == 0) ? BIT_LOW : BIT_HIGH;
    mSimulationData.SetChannel(mSettings->mInputChannel);
    mSimulationData.SetSampleRate(simulation_sample_rate);
    mSimulationData.SetInitialBitState(idle);
}

U64 FlpSimulationDataGenerator::NsToSamples(U64 ns) const {
    return (ns * static_cast<U64>(mSimulationSampleRateHz)) / 1000000000ull;
}

void FlpSimulationDataGenerator::AdvanceToSample(U64 sample) {
    U64 current = mSimulationData.GetCurrentSampleNumber();
    while (current < sample) {
        const U64 delta = sample - current;
        const U32 chunk = delta > 1000000000ull ? 1000000000u : static_cast<U32>(delta);
        mSimulationData.Advance(chunk);
        current += chunk;
    }
}

void FlpSimulationDataGenerator::EmitPulseAtNs(U64 origin, U64 time_ns) {
    AdvanceToSample(origin + NsToSamples(time_ns));
    mSimulationData.Transition();
    U64 width = NsToSamples(100);
    if (width < 1)
        width = 1;
    mSimulationData.Advance(static_cast<U32>(width));
    mSimulationData.Transition();
}

void FlpSimulationDataGenerator::EmitFlp(U16 lcw) {
    U64 origin = mSimulationData.GetCurrentSampleNumber();
    if (mBurstsSent == 0)
        origin += NsToSamples(1000000); // 1 ms of idle before the first burst

    for (int bit = 0; bit < 16; ++bit) {
        const U64 clock_ns = static_cast<U64>(bit) * 125000ull;
        EmitPulseAtNs(origin, clock_ns);
        if (lcw & (1u << bit))
            EmitPulseAtNs(origin, clock_ns + 62500ull);
    }
    EmitPulseAtNs(origin, 16ull * 125000ull);
    // Bursts are 16 ms apart, the same spacing as NLPs.
    AdvanceToSample(origin + NsToSamples(16000000ull));
}

void FlpSimulationDataGenerator::EmitNlp() {
    const U64 origin = mSimulationData.GetCurrentSampleNumber();
    EmitPulseAtNs(origin, 0);
    AdvanceToSample(origin + NsToSamples(16000000ull));
}

void FlpSimulationDataGenerator::EmitNext() {
    // Two FLP bursts advertising 802.3 + 10H + 10F, the second with Ack, then NLPs.
    if (mBurstsSent == 0)
        EmitFlp(0x0061);
    else if (mBurstsSent == 1)
        EmitFlp(0x4061);
    else
        EmitNlp();
    ++mBurstsSent;
}

U32 FlpSimulationDataGenerator::GenerateSimulationData(U64 newest_sample_requested, U32 sample_rate,
                                                       SimulationChannelDescriptor** simulation_channel) {
    const U64 adjusted =
        AnalyzerHelpers::AdjustSimulationTargetSample(newest_sample_requested, sample_rate, mSimulationSampleRateHz);

    while (mSimulationData.GetCurrentSampleNumber() < adjusted)
        EmitNext();

    *simulation_channel = &mSimulationData;
    return 1;
}
