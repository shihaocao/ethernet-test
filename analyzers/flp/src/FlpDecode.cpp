#include "FlpDecode.h"

#include <cstdio>

namespace flp {

namespace {

const uint64_t kMinPulseNs = 20;
const uint64_t kMaxPulseNs = 1500;
const uint64_t kManchesterIdleNs = 2000;
const uint64_t kBurstEndGapNs = 500000;

// Data 1 sits halfway between clocks. Dead zone is 95 µs to 100 µs.
const uint64_t kDataOneMinNs = 40000;
const uint64_t kDataOneMaxNs = 95000;
const uint64_t kDataZeroMinNs = 100000;
const uint64_t kDataZeroMaxNs = 170000;

const int kMaxPulses = 40;

void AddWord(std::string& out, const std::string& word) {
    if (word.empty())
        return;
    if (!out.empty())
        out += " ";
    out += word;
}

void AddWord(std::string& out, const char* word) {
    AddWord(out, std::string(word));
}

// D14 and D15 are always named, so a clear bit still shows up in the table.
void AddAckAndNp(std::string& out, uint16_t lcw) {
    AddWord(out, (lcw & (1u << 14)) ? "ACK" : "NACK");
    AddWord(out, (lcw & (1u << 15)) ? "YesNP" : "NoNP");
}

std::string Hex16(uint16_t value) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%04X", static_cast<unsigned>(value));
    return buf;
}

std::string SelectorName(uint16_t lcw) {
    switch (lcw & 0x1F) {
    case 1:
        return "802.3";
    case 2:
        return "802.9";
    default: {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "sel %u", static_cast<unsigned>(lcw & 0x1F));
        return buf;
    }
    }
}

// Speed and handshake bits. verbose also includes the reserved D12 and remote fault bits.
std::string BaseAbilities(uint16_t lcw, bool verbose) {
    std::string s;
    if (lcw & (1u << 5))
        AddWord(s, "10H");
    if (lcw & (1u << 6))
        AddWord(s, "10F");
    if (lcw & (1u << 7))
        AddWord(s, "100H");
    if (lcw & (1u << 8))
        AddWord(s, "100F");
    if (lcw & (1u << 9))
        AddWord(s, "T4");
    if (lcw & (1u << 10))
        AddWord(s, "Pause");
    if (lcw & (1u << 11))
        AddWord(s, "Asym");
    if (verbose && (lcw & (1u << 12)))
        AddWord(s, "D12");
    AddWord(s, (lcw & (1u << 13)) ? "YesRF" : "NoRF");
    AddAckAndNp(s, lcw);
    return s;
}

std::string NextPageBody(uint16_t lcw, bool verbose) {
    const unsigned code = lcw & 0x7FF;
    const bool message = (lcw & (1u << 13)) != 0;
    std::string s;
    if (message) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "msg %u", code);
        s = buf;
        if (code == 1)
            AddWord(s, "Null");
        else if (code == 8)
            AddWord(s, "1000T");
    } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "unf 0x%03X", code);
        s = buf;
    }
    if (verbose && (lcw & (1u << 11)))
        AddWord(s, "T");
    if (verbose && (lcw & (1u << 12)))
        AddWord(s, "Ack2");
    AddAckAndNp(s, lcw);
    return s;
}

// Medium next-page line: the code name, without toggle/ack2.
std::string NextPageMedium(uint16_t lcw) {
    const unsigned code = lcw & 0x7FF;
    const bool message = (lcw & (1u << 13)) != 0;
    std::string s;
    if (message && code == 8)
        s = "1000T";
    else if (message && code == 1)
        s = "Null";
    else if (message) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "msg %u", code);
        s = buf;
    } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "unf 0x%03X", code);
        s = buf;
    }
    AddAckAndNp(s, lcw);
    return s;
}

} // namespace

bool IsLinkPulseWidthNs(uint64_t width_ns) {
    return width_ns >= kMinPulseNs && width_ns <= kMaxPulseNs;
}

bool IsInsideManchesterNs(uint64_t idle_ns) {
    return idle_ns < kManchesterIdleNs;
}

bool GapEndsBurstNs(uint64_t gap_ns) {
    return gap_ns >= kBurstEndGapNs;
}

uint64_t SamplesToNs(uint64_t samples, uint64_t sample_rate_hz) {
    if (sample_rate_hz == 0)
        return 0;
    return (samples / sample_rate_hz) * 1000000000ull +
           (samples % sample_rate_hz) * 1000000000ull / sample_rate_hz;
}

BurstDecode DecodeBurst(const uint64_t* start_ns, int count, bool expect_next_page) {
    BurstDecode result;
    result.kind = kError;
    result.lcw = 0;
    result.is_next_page = false;
    result.expect_next_after = false;
    result.pulse_count = count;

    if (count <= 0)
        return result;

    if (count == 1) {
        result.kind = kNlp;
        return result;
    }

    if (count > kMaxPulses)
        return result;

    uint16_t lcw = 0;
    int i = 0;
    int bit = 0;
    bool ok = true;
    for (; bit < 16; ++bit) {
        // The pulse at i is a clock. One more pulse is required after it,
        // and the final clock is the last pulse in the burst.
        if (i >= count - 1) {
            ok = false;
            break;
        }
        const uint64_t dt = start_ns[i + 1] - start_ns[i];
        if (dt >= kDataOneMinNs && dt <= kDataOneMaxNs) {
            if (i + 2 >= count) {
                ok = false;
                break;
            }
            const uint64_t dt2 = start_ns[i + 2] - start_ns[i + 1];
            if (dt2 < kDataOneMinNs || dt2 > kDataOneMaxNs) {
                ok = false;
                break;
            }
            lcw = static_cast<uint16_t>(lcw | (1u << bit));
            i += 2;
        } else if (dt >= kDataZeroMinNs && dt <= kDataZeroMaxNs) {
            i += 1;
        } else {
            ok = false;
            break;
        }
    }

    if (!ok || bit != 16 || i != count - 1)
        return result;

    result.kind = kFlp;
    result.lcw = lcw;
    // A base page repeats until acknowledge. Selector 00001 is 802.3, so a
    // capture that starts mid-exchange still labels those bursts as base
    // pages. A next page whose low 5 bits happen to be 00001 (Null message
    // code 1) is labeled as a base page too.
    const bool force_base = (lcw & 0x1F) == 1;
    result.is_next_page = expect_next_page && !force_base;
    result.expect_next_after = (lcw & 0x8000) != 0;
    return result;
}

BubbleText Describe(const BurstDecode& burst) {
    BubbleText text;
    if (burst.kind == kNlp) {
        text.brief = "NLP";
        text.medium = "NLP";
        text.full = "NLP";
        return text;
    }

    if (burst.kind != kFlp) {
        text.brief = "FLP?";
        text.medium = "FLP?";
        char buf[64];
        std::snprintf(buf, sizeof(buf), "FLP? (%d %s)", burst.pulse_count,
                      burst.pulse_count == 1 ? "pulse" : "pulses");
        text.full = buf;
        return text;
    }

    if (burst.is_next_page) {
        // The short label is all that fits in the box at the 15 ms zoom.
        // Logic paints that box with the analyzer color, so the word is the ACK cue.
        text.brief = (burst.lcw & 0x4000) ? "ACK" : "NP";
        text.medium = "NP";
        AddWord(text.medium, NextPageMedium(burst.lcw));
        text.full = "NP " + Hex16(burst.lcw);
        AddWord(text.full, NextPageBody(burst.lcw, true));
        return text;
    }

    text.brief = (burst.lcw & 0x4000) ? "ACK" : "FLP";
    text.medium = "FLP";
    AddWord(text.medium, BaseAbilities(burst.lcw, false));
    text.full = "FLP " + Hex16(burst.lcw);
    AddWord(text.full, SelectorName(burst.lcw));
    AddWord(text.full, BaseAbilities(burst.lcw, true));
    return text;
}

} // namespace flp
