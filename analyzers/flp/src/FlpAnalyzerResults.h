#ifndef FLP_ANALYZER_RESULTS_H
#define FLP_ANALYZER_RESULTS_H

#include <AnalyzerResults.h>

class FlpAnalyzer;
class FlpAnalyzerSettings;

class FlpAnalyzerResults : public AnalyzerResults {
  public:
    FlpAnalyzerResults(FlpAnalyzer* analyzer, FlpAnalyzerSettings* settings);
    virtual ~FlpAnalyzerResults();

    virtual void GenerateBubbleText(U64 frame_index, Channel& channel, DisplayBase display_base);
    virtual void GenerateExportFile(const char* file, DisplayBase display_base, U32 export_type_user_id);

    virtual void GenerateFrameTabularText(U64 frame_index, DisplayBase display_base);
    virtual void GeneratePacketTabularText(U64 packet_id, DisplayBase display_base);
    virtual void GenerateTransactionTabularText(U64 transaction_id, DisplayBase display_base);

  protected:
    FlpAnalyzerSettings* mSettings;
    FlpAnalyzer* mAnalyzer;
};

#endif
