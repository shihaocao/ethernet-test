# RX extension

Receive 10BASE-T from the TLV3501 and answer an ICMP echo for our address. No code in this step.

## Pins

| Signal | GPIO | Pico header pin | Notes |
| --- | --- | --- | --- |
| TX− | GP16 | 21 | Already driven by `phy::TenBaseT`. Lower pin of the PIO side-set pair. |
| TX+ | GP17 | 22 | Next GPIO. Side-set value `0b10` raises this pin. |
| RX sense | GP18 | 24 | TLV3501 push-pull output. Header pin 23, between GP17 and GP18, is GND. |

TX stays on GP16/GP17. The pair is still 47 Ω in series with each pin, 470 Ω across the pair, then magnetics.

The TLV3501 output is one logic signal. It swings to the comparator's VCC when its inputs say the pair is positive, and to GND otherwise. Power the comparator from the Pico 3V3 pin. A 5 V supply would drive 5 V into GP18. Comparator GND goes to Pico GND. IN+ and IN− stay on the RX pair; that wiring is already the analog front end and this plan does not change it.

GP18 is the first free GPIO after the TX pair. These Pico 2W pins stay untouched because the CYW43439 uses them: GP23 (wireless on), GP24 (wireless data), GP25 (wireless chip select), GP29 (wireless clock).

## What is already true

`src/net/` builds a padded Ethernet frame with a CRC-32 FCS. `write_icmp_echo` builds an echo request (ICMP type 8). `write_gratuitous_arp` announces us.

`src/phy/tenbaset.cpp` transmits only. The PIO program is three instructions at offset 0, because `out pc, 2` jumps to absolute addresses 0, 1, and 2. A 0 on the wire is TX+ high then low. A 1 is TX+ low then high. Bytes are LSB first. The `ping` environment runs the RP2350 at 120 MHz so 20 MHz divides evenly.

Our address, from `src/targets/ping.cpp`:

- MAC `02:00:00:00:00:0a`
- IPv4 `169.254.9.10`

The capture partner is `cc:96:e5:9f:56:c5` / `169.254.9.128`.

## Receive path

One relocatable PIO program samples GP18. It does not use absolute jumps, so it can load after the TX program.

```
.wrap_target
    in pins, 1
.wrap
```

Shift right, autopush every 32 bits, so the oldest sample is the low bit of each word. Clock the state machine at 60 MHz. At 120 MHz sysclk the divider is 2. A Manchester half-symbol is 50 ns, so each half-symbol is 3 samples. Leave the GPIO input synchronizer on, and enable the Schmitt trigger.

DMA writes the FIFO into a 16 KB ring. At 60 Mbit/s of samples that ring holds about 2.2 ms. A default ping on the wire is under 100 µs, so the main loop can drain the ring without a decode IRQ. `poll()` reads from the last consumed sample up to the DMA write pointer.

Decode a burst like this:

1. Idle is a steady comparator level. A burst starts at the first edge.
2. Lock the phase to that edge. Step one sample into the new level, then take groups of 3 samples as a half-symbol.
3. A bit is two half-symbols. High-then-low is 0, low-then-high is 1, matching the transmitter. Assemble bytes LSB first.
4. A run longer than one bit time (the TP_IDL pulse, or real idle) ends the burst. A normal-link pulse is one short blip every 16 ms and never contains a preamble plus SFD, so drop anything shorter than 8 + 64 bytes.
5. Preamble is seven `0x55` bytes and SFD is `0xD5`. If that SFD does not appear, invert the bit sense and try once more. That covers a comparator wired with IN+ and IN− swapped.
6. The last four bytes are the FCS. Check them with `net::ethernet_fcs`. On failure, count it and drop the frame.

`phy::Rx10` is the new type, in `src/phy/rx10.hpp` and `src/phy/rx10.cpp`.

```cpp
bool begin(unsigned pin = 18);
// Copies one accepted frame, FCS included. Returns 0 when the ring has nothing ready.
size_t poll(uint8_t* frame, size_t cap);
```

TX and RX use two state machines. TX keeps PIO offset 0. RX loads at the next free offset on that same PIO if it fits, otherwise on another PIO block. `TenBaseT::begin` today only looks at `pio0` and `pio1`. The RP2350 also has `pio2`. While placing RX, teach the search about `pio2` so the CYW43439 SPI program, which `digitalWrite(LED_BUILTIN)` can start, still has a block left. Claim our state machines before the first LED write.

Our own transmit is a different pair, so GP18 should not see it. If the bench loops TX back into RX, drop any frame whose source MAC is ours.

## Reply

A host `ping 169.254.9.10` sends ARP before ICMP. Answer both, or the ping never leaves the host.

Accept a frame when the FCS is good and all of these hold:

- Destination MAC is `02:00:00:00:00:0a`.
- For ARP: ethertype `0x0806`, opcode request, target protocol address `169.254.9.10`. Reply opcode 2, sender hardware and protocol address are ours, target hardware and protocol address are copied from the request.
- For ICMP: ethertype `0x0800`, IPv4 IHL 5, header checksum good, protocol 1, destination `169.254.9.10`, ICMP type 8 code 0, ICMP checksum good. The ICMP length comes from the IPv4 total length, so the Ethernet pad is not part of the checksum.

The echo reply swaps MAC addresses and IP addresses, sets ICMP type to 0, and copies identifier, sequence, and payload. Recompute the IPv4 checksum, the ICMP checksum, and the FCS. Send it with the existing `TenBaseT::send`, which already waits out the interframe gap.

`write_icmp_echo` stays a request builder. Add `write_icmp_echo_reply` and `write_arp_reply` beside it. The ICMP data cap is 64 bytes today. A default Linux ping carries 56 data bytes, which fits. Raise the cap to 128 so a slightly larger ping still fits in the reply buffer.

Parsing lives in `src/net/packet.hpp` as a small result struct (MACs, ethertype, L3 span inside the frame, FCS ok). The echo target is the only caller.

## Firmware

New environment `echo` in `platformio.ini`, same 120 MHz clock and the same source filter shape as `ping`. New file `src/targets/echo.cpp`.

That target:

- Starts TX, then RX, then the LED.
- Sends one gratuitous ARP per second so the peer can learn our MAC even before it asks.
- Calls `tx.poll()` for our link pulses and `rx.poll()` every loop.
- On an ARP request or an echo request for `169.254.9.10`, sends the reply and prints one line: kind, peer address, ICMP sequence, frame length.
- Does not send its own echo requests. `ping` remains the transmit-only generator.

## How to check

Host-side, before flashing:

- Build an echo request with `write_icmp_echo`, parse it, build the reply, and check swapped MAC and IP, type 0, same id and sequence and payload, and all three checksums.
- Same round trip for an ARP request for `169.254.9.10`.
- Feed the decoder a packed sample buffer of a known frame (preamble, Manchester bits at 3 samples per half-symbol, both polarities) and check the bytes and the FCS. A short pulse must produce no frame.

On the bench, after that:

```bash
.venv/bin/pio run -e echo -t upload
```

From the peer, with the link forced to 10 Mbit/s:

```bash
ping -c 3 169.254.9.10
```

The peer prints replies. USB serial on the Pico prints one line per request. The onboard LED toggles on each reply. A request for any other IPv4 address produces no reply.

## Order

1. Reply builders and the parser, with the host checks.
2. Sampler, DMA ring, and decoder, with the packed-sample check.
3. `echo` target wiring ARP and ICMP replies to `TenBaseT::send`.
4. Build `pio run -e echo`.
5. Flash and ping from the peer.

## Out of scope

UDP, TCP, and any other ICMP type. Receiving on a second comparator pin. A full lwIP netif. Promiscuous capture. Answering pings that are not addressed to `169.254.9.10`.
