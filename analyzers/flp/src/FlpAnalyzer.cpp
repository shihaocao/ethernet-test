#include "FlpAnalyzer.h"

#include "FlpDecode.h"

#include <AnalyzerChannelData.h>

FlpAnalyzer::FlpAnalyzer() : Analyzer2(), mSettings(), mData(0), mSampleRateHz(0), mActive(BIT_HIGH), mExpectNextPage(false), mSimulationInitialized(false) {
    SetAnalyzerSettings(&mSettings);
#ifdef LOGIC2
    UseFrameV2();
#endif
}

FlpAnalyzer::~FlpAnalyzer() {
    KillThread();
}

void FlpAnalyzer::SetupResults() {
    mResults.reset(new FlpAnalyzerResults(this, &mSettings));
    SetAnalyzerResults(mResults.get());
    mResults->AddChannelBubblesWillAppearOn(mSettings.mInputChannel);
}

void FlpAnalyzer::EmitBurst(const std::vector<Pulse>& pulses) {
    if (pulses.empty())
        return;

    std::vector<uint64_t> starts;
    starts.reserve(pulses.size());
    for (size_t i = 0; i < pulses.size(); ++i)
        starts.push_back(flp::SamplesToNs(pulses[i].start, mSampleRateHz));

    const flp::BurstDecode decoded = flp::DecodeBurst(starts.data(), static_cast<int>(starts.size()), mExpectNextPage);
    mExpectNextPage = decoded.expect_next_after;

    // An NLP is one pulse every 16 ms for the whole time the link stays up.
    // Recording each one fills the data table and does not change the page decode.
    if (decoded.kind == flp::kNlp && !mSettings.mDecodeNlps) {
        ReportProgress(pulses.back().end);
        return;
    }

    const AnalyzerResults::MarkerType marker =
        (decoded.kind == flp::kError) ? AnalyzerResults::ErrorDot : AnalyzerResults::Dot;
    for (size_t i = 0; i < pulses.size(); ++i)
        mResults->AddMarker(pulses[i].start, marker, mSettings.mInputChannel);

    Frame frame;
    frame.mStartingSampleInclusive = static_cast<S64>(pulses.front().start);
    frame.mEndingSampleInclusive = static_cast<S64>(pulses.back().end);
    // Logic shades a bubble from mFlags only. Warning is the amber box, which
    // marks an acknowledged page against the normal box used for NACK.
    frame.mFlags = 0;
    if (decoded.kind == flp::kError)
        frame.mFlags = DISPLAY_AS_ERROR_FLAG;
    else if (decoded.kind == flp::kFlp && (decoded.lcw & 0x4000) != 0)
        frame.mFlags = DISPLAY_AS_WARNING_FLAG;
    frame.mData2 = static_cast<U64>(decoded.kind);
    if (decoded.is_next_page)
        frame.mData2 |= flp::kNextPageBit;
    if (decoded.kind == flp::kError)
        frame.mData1 = static_cast<U64>(decoded.pulse_count);
    else
        frame.mData1 = decoded.lcw;
    frame.mType = 0;
    mResults->AddFrame(frame);

#ifdef LOGIC2
    const flp::BubbleText text = flp::Describe(decoded);
    FrameV2 frame_v2;
    const char* type = "error";
    if (decoded.kind == flp::kNlp)
        type = "nlp";
    else if (decoded.kind == flp::kFlp)
        type = "flp";
    frame_v2.AddString("summary", text.full.c_str());
    if (decoded.kind == flp::kFlp) {
        frame_v2.AddInteger("lcw", decoded.lcw);
        frame_v2.AddString("page", decoded.is_next_page ? "next" : "base");
    } else if (decoded.kind == flp::kError) {
        frame_v2.AddInteger("pulses", decoded.pulse_count);
    }
    mResults->AddFrameV2(frame_v2, type, pulses.front().start, pulses.back().end);
#endif

    mResults->CommitResults();
    ReportProgress(pulses.back().end);
}

void FlpAnalyzer::SkipManchester() {
    // Caller is at idle. The next edge starts another short pulse. Walk edges
    // until the idle that follows a pulse is long enough to leave the frame.
    for (;;) {
        CheckIfThreadShouldExit();
        mData->AdvanceToNextEdge();
        mData->AdvanceToNextEdge();
        const U64 next = mData->GetSampleOfNextEdge();
        const uint64_t idle_ns = flp::SamplesToNs(next - mData->GetSampleNumber(), mSampleRateHz);
        if (!flp::IsInsideManchesterNs(idle_ns))
            return;
    }
}

void FlpAnalyzer::WorkerThread() {
    mSampleRateHz = GetSampleRate();
    mData = GetAnalyzerChannelData(mSettings.mInputChannel);
    mActive = (mSettings.mPolarity == 0) ? BIT_HIGH : BIT_LOW;
    mExpectNextPage = false;

    std::vector<Pulse> burst;
    bool have_open = false;
    Pulse open;
    open.start = 0;
    open.end = 0;

    try {
        if (mData->GetBitState() == mActive)
            mData->AdvanceToNextEdge();

        for (;;) {
            CheckIfThreadShouldExit();

            if (mData->GetBitState() == mActive)
                mData->AdvanceToNextEdge();

            mData->AdvanceToNextEdge();
            open.start = mData->GetSampleNumber();
            open.end = mData->GetSampleOfNextEdge();
            mData->AdvanceToNextEdge();
            have_open = true;

            const U64 next = mData->GetSampleOfNextEdge();
            const U64 gap_samples = next - open.end;
            have_open = false;

            const uint64_t width_ns = flp::SamplesToNs(open.end - open.start, mSampleRateHz);
            const uint64_t gap_ns = flp::SamplesToNs(gap_samples, mSampleRateHz);
            const bool link_pulse = flp::IsLinkPulseWidthNs(width_ns);
            const bool manchester = flp::IsInsideManchesterNs(gap_ns);

            if (!link_pulse || manchester) {
                // A Manchester frame on this same pair. Drop it, and close any
                // link burst that was still open so the two do not merge.
                if (!burst.empty()) {
                    EmitBurst(burst);
                    burst.clear();
                }
                if (manchester)
                    SkipManchester();
                continue;
            }

            burst.push_back(open);
            if (flp::GapEndsBurstNs(gap_ns) || burst.size() >= 48) {
                EmitBurst(burst);
                burst.clear();
            }
        }
    } catch (...) {
        if (have_open && flp::IsLinkPulseWidthNs(flp::SamplesToNs(open.end - open.start, mSampleRateHz)))
            burst.push_back(open);
        if (!burst.empty())
            EmitBurst(burst);
        throw;
    }
}

bool FlpAnalyzer::NeedsRerun() {
    return false;
}

U32 FlpAnalyzer::GenerateSimulationData(U64 newest_sample_requested, U32 sample_rate,
                                       SimulationChannelDescriptor** simulation_channels) {
    if (!mSimulationInitialized) {
        mSimulationDataGenerator.Initialize(GetSimulationSampleRate(), &mSettings);
        mSimulationInitialized = true;
    }
    return mSimulationDataGenerator.GenerateSimulationData(newest_sample_requested, sample_rate, simulation_channels);
}

U32 FlpAnalyzer::GetMinimumSampleRateHz() {
    // A 100 ns link pulse needs a handful of samples. 40 MS/s leaves 4.
    return 40000000;
}

const char* FlpAnalyzer::GetAnalyzerName() const {
    return "FLP";
}

const char* GetAnalyzerName() {
    return "FLP";
}

Analyzer* CreateAnalyzer() {
    return new FlpAnalyzer();
}

void DestroyAnalyzer(Analyzer* analyzer) {
    delete analyzer;
}
