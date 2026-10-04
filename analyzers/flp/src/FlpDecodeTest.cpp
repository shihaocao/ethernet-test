#include "FlpDecode.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_failed = 0;

void Check(bool cond, const char* expr, int line) {
    if (!cond) {
        std::fprintf(stderr, "FAIL %s:%d: %s\n", "FlpDecodeTest.cpp", line, expr);
        ++g_failed;
    }
}

#define CHECK(cond) Check((cond), #cond, __LINE__)

std::vector<uint64_t> Ideal(uint16_t lcw) {
    std::vector<uint64_t> t;
    for (int bit = 0; bit < 16; ++bit) {
        const uint64_t clock = static_cast<uint64_t>(bit) * 125000ull;
        t.push_back(clock);
        if (lcw & (1u << bit))
            t.push_back(clock + 62500ull);
    }
    t.push_back(16ull * 125000ull);
    return t;
}

} // namespace

int main() {
    CHECK(flp::IsLinkPulseWidthNs(100));
    CHECK(flp::IsLinkPulseWidthNs(20));
    CHECK(flp::IsLinkPulseWidthNs(1500));
    CHECK(!flp::IsLinkPulseWidthNs(19));
    CHECK(!flp::IsLinkPulseWidthNs(1501));

    CHECK(flp::IsInsideManchesterNs(100));
    CHECK(flp::IsInsideManchesterNs(1999));
    CHECK(!flp::IsInsideManchesterNs(2000));

    CHECK(!flp::GapEndsBurstNs(499999));
    CHECK(flp::GapEndsBurstNs(500000));

    CHECK(flp::SamplesToNs(4, 40000000ull) == 100ull);
    CHECK(flp::SamplesToNs(100, 100000000ull) == 1000ull);
    CHECK(flp::SamplesToNs(20000000000ull, 100000000ull) == 200000000000ull);

    // Bit 0 and bit 6 set. Clocks every 125 µs, data pulses 62.5 µs after those clocks.
    const uint16_t bits_0_and_6 = static_cast<uint16_t>(1u | (1u << 6));
    const std::vector<uint64_t> hand = Ideal(bits_0_and_6);
    CHECK(hand.size() == 19u);
    CHECK(hand[0] == 0ull);
    CHECK(hand[1] == 62500ull);
    CHECK(hand[2] == 125000ull);
    CHECK(hand[7] == 750000ull);
    CHECK(hand[8] == 812500ull);
    CHECK(hand[18] == 2000000ull);

    const flp::BurstDecode decoded = flp::DecodeBurst(hand.data(), static_cast<int>(hand.size()), false);
    CHECK(decoded.kind == flp::kFlp);
    CHECK(decoded.lcw == bits_0_and_6);
    CHECK(!decoded.is_next_page);
    CHECK(!decoded.expect_next_after);

    for (unsigned lcw = 0; lcw <= 0xFFFF; ++lcw) {
        const std::vector<uint64_t> pulses = Ideal(static_cast<uint16_t>(lcw));
        const flp::BurstDecode round = flp::DecodeBurst(pulses.data(), static_cast<int>(pulses.size()), false);
        if (round.kind != flp::kFlp || round.lcw != lcw) {
            std::fprintf(stderr, "FAIL round trip lcw 0x%04X kind %d got 0x%04X\n", lcw, round.kind, round.lcw);
            ++g_failed;
            break;
        }
        const bool want_next = (lcw & 0x8000) != 0;
        if (round.expect_next_after != want_next) {
            std::fprintf(stderr, "FAIL next-page state lcw 0x%04X\n", lcw);
            ++g_failed;
            break;
        }
    }

    uint64_t nlp = 0;
    flp::BurstDecode nlp_burst = flp::DecodeBurst(&nlp, 1, true);
    CHECK(nlp_burst.kind == flp::kNlp);
    CHECK(!nlp_burst.expect_next_after);
    CHECK(flp::Describe(nlp_burst).brief == "NLP");

    flp::BurstDecode empty = flp::DecodeBurst(0, 0, false);
    CHECK(empty.kind == flp::kError);

    // 17 clocks, every interval a data 0. Boundaries and the dead zone.
    std::vector<uint64_t> clocks(17);
    for (int i = 0; i < 17; ++i)
        clocks[i] = static_cast<uint64_t>(i) * 170000ull;
    flp::BurstDecode wide = flp::DecodeBurst(clocks.data(), 17, false);
    CHECK(wide.kind == flp::kFlp);
    CHECK(wide.lcw == 0);

    for (int i = 0; i < 17; ++i)
        clocks[i] = static_cast<uint64_t>(i) * 100000ull;
    flp::BurstDecode tight = flp::DecodeBurst(clocks.data(), 17, false);
    CHECK(tight.kind == flp::kFlp);
    CHECK(tight.lcw == 0);

    for (int i = 0; i < 17; ++i)
        clocks[i] = static_cast<uint64_t>(i) * 99999ull;
    CHECK(flp::DecodeBurst(clocks.data(), 17, false).kind == flp::kError);

    for (int i = 0; i < 17; ++i)
        clocks[i] = static_cast<uint64_t>(i) * 171000ull;
    CHECK(flp::DecodeBurst(clocks.data(), 17, false).kind == flp::kError);

    std::vector<uint64_t> ones;
    uint64_t cursor = 0;
    for (int bit = 0; bit < 16; ++bit) {
        ones.push_back(cursor);
        cursor += 95000ull;
        ones.push_back(cursor);
        cursor += 95000ull;
    }
    ones.push_back(cursor);
    flp::BurstDecode all_ones = flp::DecodeBurst(ones.data(), static_cast<int>(ones.size()), false);
    CHECK(all_ones.kind == flp::kFlp);
    CHECK(all_ones.lcw == 0xFFFF);
    CHECK(all_ones.expect_next_after);

    ones.clear();
    cursor = 0;
    for (int bit = 0; bit < 16; ++bit) {
        ones.push_back(cursor);
        cursor += 95001ull;
        ones.push_back(cursor);
        cursor += 95001ull;
    }
    ones.push_back(cursor);
    CHECK(flp::DecodeBurst(ones.data(), static_cast<int>(ones.size()), false).kind == flp::kError);

    std::vector<uint64_t> too_many(41, 0);
    for (int i = 0; i < 41; ++i)
        too_many[i] = static_cast<uint64_t>(i) * 125000ull;
    flp::BurstDecode many = flp::DecodeBurst(too_many.data(), 41, true);
    CHECK(many.kind == flp::kError);
    CHECK(!many.expect_next_after);
    CHECK(flp::Describe(many).full == "FLP? (41 pulses)");

    // 10H + 10F + 802.3 + Ack, the shape of a finished 10BASE-T exchange.
    const std::vector<uint64_t> advertised = Ideal(0x4061);
    flp::BurstDecode base = flp::DecodeBurst(advertised.data(), static_cast<int>(advertised.size()), false);
    CHECK(base.kind == flp::kFlp);
    CHECK(base.lcw == 0x4061);
    const flp::BubbleText base_text = flp::Describe(base);
    CHECK(base_text.brief == "ACK");
    CHECK(base_text.medium.find("10H") != std::string::npos);
    CHECK(base_text.medium.find("10F") != std::string::npos);
    CHECK(base_text.medium.find("ACK") != std::string::npos);
    CHECK(base_text.medium.find("NACK") == std::string::npos);
    CHECK(base_text.medium.find("NoNP") != std::string::npos);
    CHECK(base_text.medium.find("YesNP") == std::string::npos);
    CHECK(base_text.medium.find("NoRF") != std::string::npos);
    CHECK(base_text.medium.find("YesRF") == std::string::npos);
    CHECK(base_text.medium.find("100H") == std::string::npos);
    CHECK(base_text.full.find("0x4061") != std::string::npos);
    CHECK(base_text.full.find("802.3") != std::string::npos);
    CHECK(base_text.full.find("ACK") != std::string::npos);
    CHECK(base_text.full.find("NoNP") != std::string::npos);

    // The partner page from flip-bursts2: abilities set, acknowledge and next page clear.
    const std::vector<uint64_t> partner = Ideal(0x0461);
    const flp::BubbleText partner_text =
        flp::Describe(flp::DecodeBurst(partner.data(), static_cast<int>(partner.size()), false));
    CHECK(partner_text.full.find("10H") != std::string::npos);
    CHECK(partner_text.full.find("10F") != std::string::npos);
    CHECK(partner_text.full.find("Pause") != std::string::npos);
    CHECK(partner_text.brief == "FLP");
    CHECK(partner_text.full.find("NoRF") != std::string::npos);
    CHECK(partner_text.full.find("YesRF") == std::string::npos);
    CHECK(partner_text.full.find("NACK") != std::string::npos);
    CHECK(partner_text.full.find("NoNP") != std::string::npos);
    CHECK(partner_text.full.find("YesNP") == std::string::npos);
    CHECK(partner_text.medium.find("NACK") != std::string::npos);
    CHECK(partner_text.medium.find("NoNP") != std::string::npos);

    const std::vector<uint64_t> faulted = Ideal(0x2461);
    const flp::BubbleText fault_text =
        flp::Describe(flp::DecodeBurst(faulted.data(), static_cast<int>(faulted.size()), false));
    CHECK(fault_text.full.find("YesRF") != std::string::npos);
    CHECK(fault_text.full.find("NoRF") == std::string::npos);

    // NP set: the following burst is a next page, unless it still looks like an 802.3 base page.
    const std::vector<uint64_t> with_np = Ideal(0x8061);
    flp::BurstDecode np_base = flp::DecodeBurst(with_np.data(), static_cast<int>(with_np.size()), false);
    CHECK(np_base.expect_next_after);
    CHECK(!np_base.is_next_page);

    const std::vector<uint64_t> page_1000t = Ideal(0x2008); // message page, code 8
    flp::BurstDecode next = flp::DecodeBurst(page_1000t.data(), static_cast<int>(page_1000t.size()), true);
    CHECK(next.kind == flp::kFlp);
    CHECK(next.is_next_page);
    CHECK(flp::Describe(next).brief == "NP");
    CHECK(flp::Describe(next).medium.find("1000T") != std::string::npos);
    CHECK(flp::Describe(next).full.find("msg 8") != std::string::npos);

    flp::BurstDecode forced = flp::DecodeBurst(advertised.data(), static_cast<int>(advertised.size()), true);
    CHECK(forced.kind == flp::kFlp);
    CHECK(!forced.is_next_page);
    CHECK(flp::Describe(forced).brief == "ACK");

    if (g_failed != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("flp decode ok\n");
    return 0;
}
