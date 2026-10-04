# Captures

One folder per machine. Logic 2 exports the same CSV on Linux and Windows, so a capture from either host can sit next to the other.

| Folder | Machine |
| --- | --- |
| `t14s/` | Linux ThinkPad T14s (`scao-u-t14s`). Saleae Logic 8, Logic 2.4.46. |
| `dell/` | Windows Dell. Put that machine's exports here. |

Export from the Logic 2 data table after adding a Manchester analyzer. Keep the column header (`name`, `type`, `start_time`, `duration`, `data`). Name the analyzer after the pair it is clipped to, as in `Pin1/2` and `Pin3/6`, so a file that contains both directions stays readable.
