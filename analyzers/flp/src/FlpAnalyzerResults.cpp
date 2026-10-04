#include "FlpAnalyzerResults.h"

#include "FlpAnalyzer.h"
#include "FlpAnalyzerSettings.h"
#include "FlpDecode.h"

#include <AnalyzerHelpers.h>

#include <cstdio>

namespace {

flp::BurstDecode BurstFromFrame(const Frame& frame) {
    flp::BurstDecode burst;
    burst.kind = static_cast<int>(frame.mData2 & 0xFF);
    burst.is_next_page = (frame.mData2 & flp::kNextPageBit) != 0;
    burst.expect_next_after = false;
    if (burst.kind == flp::kError) {
        burst.lcw = 0;
        burst.pulse_count = static_cast<int>(frame.mData1);
    } else {
        burst.lcw = static_cast<uint16_t>(frame.mData1);
        burst.pulse_count = 0;
    }
    return burst;
}

const char* KindName(int kind) {
    if (kind == flp::kNlp)
        return "NLP";
    if (kind == flp::kFlp)
        return "FLP";
    return "FLP?";
}

} // namespace

FlpAnalyzerResults::FlpAnalyzerResults(FlpAnalyzer* analyzer, FlpAnalyzerSettings* settings)
    : AnalyzerResults(), mSettings(settings), mAnalyzer(analyzer) {
}

FlpAnalyzerResults::~FlpAnalyzerResults() {
}

void FlpAnalyzerResults::GenerateBubbleText(U64 frame_index, Channel& channel, DisplayBase display_base) {
    (void)channel;
    (void)display_base;
    ClearResultStrings();
    const Frame frame = GetFrame(frame_index);
    const flp::BubbleText text = flp::Describe(BurstFromFrame(frame));

    AddResultString(text.brief.c_str());
    if (text.medium != text.brief)
        AddResultString(text.medium.c_str());
    if (text.full != text.medium && text.full != text.brief)
        AddResultString(text.full.c_str());
}

void FlpAnalyzerResults::GenerateExportFile(const char* file, DisplayBase display_base, U32 export_type_user_id) {
    (void)display_base;
    (void)export_type_user_id;

    FILE* out = std::fopen(file, "w");
    if (out == 0)
        return;

    const U64 trigger_sample = mAnalyzer->GetTriggerSample();
    const U32 sample_rate = mAnalyzer->GetSampleRate();

    std::fprintf(out, "Time [s],Kind,LCW,Page,Summary\n");

    const U64 num_frames = GetNumFrames();
    for (U32 i = 0; i < num_frames; ++i) {
        const Frame frame = GetFrame(i);
        const flp::BurstDecode burst = BurstFromFrame(frame);
        const flp::BubbleText text = flp::Describe(burst);

        char time_str[128];
        AnalyzerHelpers::GetTimeString(frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128);

        std::fprintf(out, "%s,%s,", time_str, KindName(burst.kind));
        if (burst.kind == flp::kFlp) {
            char lcw_str[16];
            AnalyzerHelpers::GetNumberString(burst.lcw, Hexadecimal, 16, lcw_str, 16);
            std::fputs(lcw_str, out);
        }
        std::fputc(',', out);
        if (burst.kind == flp::kFlp)
            std::fputs(burst.is_next_page ? "next" : "base", out);
        std::fprintf(out, ",%s\n", text.full.c_str());

        if (UpdateExportProgressAndCheckForCancel(i, num_frames)) {
            std::fclose(out);
            return;
        }
    }

    std::fclose(out);
}

void FlpAnalyzerResults::GenerateFrameTabularText(U64 frame_index, DisplayBase display_base) {
#ifdef SUPPORTS_PROTOCOL_SEARCH
    (void)display_base;
    ClearTabularText();
    const Frame frame = GetFrame(frame_index);
    const flp::BubbleText text = flp::Describe(BurstFromFrame(frame));
    AddTabularText(text.full.c_str());
#endif
}

void FlpAnalyzerResults::GeneratePacketTabularText(U64 packet_id, DisplayBase display_base) {
    (void)packet_id;
    (void)display_base;
}

void FlpAnalyzerResults::GenerateTransactionTabularText(U64 transaction_id, DisplayBase display_base) {
    (void)transaction_id;
    (void)display_base;
}
