#ifndef FLP_DECODE_H
#define FLP_DECODE_H

#include <cstddef>
#include <cstdint>
#include <string>

// Clause 28 Fast Link Pulse (FLP) and Normal Link Pulse (NLP) decode.
//
// An FLP burst is 17 clock pulses, 125 µs apart, with an optional data
// pulse halfway between clocks. The 16 data bits are the link code word,
// LSB first. One isolated pulse is an NLP.
//
// The windows are wider than the spec (±7 µs data, ±14 µs clock) because a
// logic threshold slides the measured edges of a ~100 ns pulse.

namespace flp {

enum BurstKind {
    kNlp = 0,
    kFlp = 1,
    kError = 2
};

// Stored in Frame::mData2 along with BurstKind in the low 8 bits.
static const uint64_t kNextPageBit = 0x100;

struct BurstDecode {
    int kind;
    uint16_t lcw;
    bool is_next_page;
    bool expect_next_after;
    int pulse_count;
};

struct BubbleText {
    std::string brief;
    std::string medium;
    std::string full;
};

// start_ns[i] is the start of pulse i, in nanoseconds, one burst only.
BurstDecode DecodeBurst(const uint64_t* start_ns, int count, bool expect_next_page);

BubbleText Describe(const BurstDecode& burst);

bool IsLinkPulseWidthNs(uint64_t width_ns);
bool IsInsideManchesterNs(uint64_t idle_ns);
bool GapEndsBurstNs(uint64_t gap_ns);

uint64_t SamplesToNs(uint64_t samples, uint64_t sample_rate_hz);

} // namespace flp

#endif
