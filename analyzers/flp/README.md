# FLP analyzer

Logic 2 low-level analyzer for 10BASE-T link pulses on one pair. It draws a bubble for each Fast Link Pulse (FLP) burst. Normal Link Pulses (one pulse about every 16 ms once the link is up) are skipped unless **Decode NLPs** is checked. Manchester frames on that same pair are left alone.

An FLP burst is 17 clock pulses, 125 µs apart. A data 1 is an extra pulse halfway between two clocks. The 16 bits are the Clause 28 link code word, least significant bit first. One pulse by itself is an NLP.

| Bubble | Meaning |
| --- | --- |
| `NLP` | One link pulse. Shown only when **Decode NLPs** is checked. |
| `FLP 10H 10F NoRF ACK NoNP` | Base page. `10H`/`10F` are 10BASE-T half and full, `100H`/`100F` are 100BASE-TX. `YesRF` or `NoRF` is remote fault. `ACK` or `NACK` is the acknowledge bit. `YesNP` or `NoNP` is the next-page bit. The long bubble includes the code word, for example `FLP 0x0461 802.3 10H 10F Pause NoRF NACK NoNP`. Zoomed out, the box only fits one word: `ACK` when the acknowledge bit is set, and `FLP` when it is not. Logic fills the box with the analyzer color either way. |
| `NP 1000T` | Next page. Message code 8 is 1000BASE-T. Message code 1 is a Null page. |
| `FLP?` | Pulses in a burst, but the spacing is not a link code word. |

A peer forced to 10 Mbit with autonegotiation off sends only NLPs, about every 16 ms. The other PHY still sends FLPs until it parallel-detects those NLPs, then it sends NLPs too. Parallel detection settles half duplex. A PHY that is only advertising 10 Mbit, with autonegotiation still on, keeps sending FLPs.

The data table has one row per burst: `summary`, and for an FLP the `lcw` and `page` (`base` or `next`). Export writes `Time [s],Kind,LCW,Page,Summary`.

## Build

Linux, from the repo root:

```
cmake -S analyzers/flp -B analyzers/flp/build -DCMAKE_BUILD_TYPE=Release
cmake --build analyzers/flp/build
```

The plugin is `analyzers/flp/build/Analyzers/FlpAnalyzer.so`.

On the Windows Dell, the same commands in a Visual Studio x64 prompt produce `analyzers\flp\build\Analyzers\Release\FlpAnalyzer.dll` (the `Release` folder is omitted with Ninja). CMake downloads the Saleae Analyzer SDK.

`flp_decode_test` checks the link-code-word decode. `ctest --test-dir analyzers/flp/build` runs it.

## Load into Logic 2

1. Edit → Settings (on macOS, Logic 2 → Settings).
2. Scroll to Custom Low Level Analyzers and choose the folder that contains `FlpAnalyzer.so` or `FlpAnalyzer.dll`. That folder should not contain other libraries.
3. Restart Logic 2.
4. Add the **FLP** analyzer once per pair (pins 1–2 and pins 3–6 are separate channels).

Polarity defaults to active high, which is a positive link pulse. If a channel stays empty, switch that analyzer to active low. The Manchester decode of this dongle needed the inverted setting before the preamble read `0x55`, so active low is the likely match on that capture. Sample at 100 MS/s. The analyzer asks simulation for 40 MS/s.

Simulation draws two FLP bursts (802.3, 10 half, 10 full, the second with Ack) and then NLPs every 16 ms.
