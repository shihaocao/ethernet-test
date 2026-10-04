# Logic 2 presets

`EthernetDebugging.logic2Preset` is the Logic 8 setup for this bench. Digital channels 0 and 1, 100 MS/s, 3 GB buffer. Analyzers:

| Name | Channel | Settings |
| --- | --- | --- |
| `FLP1/2` | 0 | Active high, NLPs off |
| `FLP3/6` | 1 | Active high, NLPs off |
| `Manchester1/2` | 0 | 10 Mbit/s, 8 bits, LSB first, negative edge is binary zero |
| `Manchester3/6` | 1 | Same Manchester settings |

In Logic 2, use File → Presets → Import, then Load. The file is schema version 19, which matches Logic 2.4.46.
