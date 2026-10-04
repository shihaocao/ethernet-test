#ifndef FLP_ANALYZER_H
#define FLP_ANALYZER_H

#include "FlpAnalyzerResults.h"
#include "FlpAnalyzerSettings.h"
#include "FlpSimulationDataGenerator.h"

#include <Analyzer.h>

#include <memory>
#include <vector>

class ANALYZER_EXPORT FlpAnalyzer : public Analyzer2 {
  public:
    FlpAnalyzer();
    virtual ~FlpAnalyzer();

    virtual void SetupResults();
    virtual void WorkerThread();

    virtual U32 GenerateSimulationData(U64 newest_sample_requested, U32 sample_rate,
                                       SimulationChannelDescriptor** simulation_channels);
    virtual U32 GetMinimumSampleRateHz();

    virtual const char* GetAnalyzerName() const;
    virtual bool NeedsRerun();

  protected:
    struct Pulse {
        U64 start;
        U64 end;
    };

    void EmitBurst(const std::vector<Pulse>& pulses);
    void SkipManchester();

    FlpAnalyzerSettings mSettings;
    std::unique_ptr<FlpAnalyzerResults> mResults;
    AnalyzerChannelData* mData;
    U64 mSampleRateHz;
    BitState mActive;
    bool mExpectNextPage;

    FlpSimulationDataGenerator mSimulationDataGenerator;
    bool mSimulationInitialized;
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer();
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer(Analyzer* analyzer);

#endif
