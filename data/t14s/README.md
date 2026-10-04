# T14s captures

Taken 2026-10-04 on `scao-u-t14s` with a Saleae Logic 8 and Logic 2.4.46. The dongle was `enx6c6e072f7dc4` (`6c:6e:07:2f:7d:c4`, ASIX AX88179A), cabled straight into the other computer's dongle. The partner was forced to 10 Mbit. At capture time this dongle was bound to `cdc_ncm` and the link was up. `ethtool` reported 10 Mb/s. Its duplex field is not a measurement.

Logic 2 has no Ethernet frame decoder. These files are the built-in Manchester analyzer:

- Mode: Manchester (IEEE 802.3), not differential and not bi-phase mark or space
- Bit rate: 10000000
- 8 bits, least significant bit first
- One analyzer per pair

10BASE-T has no forward error correction. Each frame ends with a 4-byte CRC-32, which can only detect a bad frame.

## Files

`tx-decode1.csv` is the first export of this dongle's transmit pair, before the channel was inverted. Seven `0xAA` bytes and then `0x2A` are `0x55` and the start-of-frame byte `0xD5` with every bit flipped.

`tx-decode2.csv` is the same transmit pair after inverting the channel. Ten frames, every checksum valid.

| Time | Frame |
| --- | --- |
| 0.360 s, then every 1 s through 8.369 s | ICMP echo request, id 13, sequence 898 through 906. 102 bytes on the wire including the checksum. |
| 8.657 s | ARP request. 64 bytes including the checksum. |

Source `6c:6e:07:2f:7d:c4` / `169.254.9.129`. Destination `cc:96:e5:9f:56:c5` / `169.254.9.128`. Each frame starts `55 55 55 55 55 55 55 D5`.

`tx-decode3` is both pairs from that same ping run. The `name` column is the pair.

| Pair | Who is transmitting | What is on it |
| --- | --- | --- |
| `Pin1/2` | This dongle, `6c:6e:07:2f:7d:c4` | The nine echo requests and the ARP request. Same bytes as `tx-decode2.csv`. |
| `Pin3/6` | The other dongle, `cc:96:e5:9f:56:c5` | DHCP from `0.0.0.0:68` to `255.255.255.255:67` at 0.198 s, then echo replies 898 through 906, then the ARP reply at 8.657 s. |

On 10BASE-T, MDI transmits on pins 1–2 and MDI-X transmits on pins 3–6. This dongle is MDI. The other computer is MDI-X.

`ethtool enx6c6e072f7dc4` prints `MDI-X: Unknown`. `cdc_ncm` does not report the crossover, and `ax88179_178a` does not either, so the NIC cannot confirm the pair assignment. `MDI-X: off` would mean MDI. `MDI-X: on` would mean MDI-X.
