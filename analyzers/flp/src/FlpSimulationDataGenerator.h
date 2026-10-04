#ifndef FLP_SIMULATION_DATA_GENERATOR_H
#define FLP_SIMULATION_DATA_GENERATOR_H

#include <SimulationChannelDescriptor.h>

class FlpAnalyzerSettings;

class FlpSimulationDataGenerator {
  public:
    FlpSimulationDataGenerator();
    ~FlpSimulationDataGenerator();

    void Initialize(U32 simulation_sample_rate, FlpAnalyzerSettings* settings);
    U32 GenerateSimulationData(U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel);

  protected:
    void EmitNext();
    void EmitFlp(U16 lcw);
    void EmitNlp();
    void EmitPulseAtNs(U64 origin, U64 time_ns);
    void AdvanceToSample(U64 sample);
    U64 NsToSamples(U64 ns) const;

    FlpAnalyzerSettings* mSettings;
    U32 mSimulationSampleRateHz;
    U32 mBurstsSent;
    SimulationChannelDescriptor mSimulationData;
};

#endif
