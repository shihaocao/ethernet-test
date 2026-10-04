# AX88179A dongle: 10/100 full duplex, link stays down

Captured 2026-10-04 on `scao-u-t14s` before unplugging the adapter.
Replug clears the ethtool advertisement. The register dump below is from the plugged-in session and will not match a fresh insert until the same `advertise` command is run again.

## Hardware

| Item | Value |
| --- | --- |
| Interface | `enx6c6e072f7dc4` |
| MAC | `6c:6e:07:2f:7d:c4` |
| USB | bus 004, device 006, port `4-2` (`/sys/devices/pci0000:00/0000:00:08.3/0000:c5:00.0/usb4/4-2`) |
| USB ID | `0b95:1790` (lsusb name: AX88179 Gigabit Ethernet) |
| Product string | ASIX AX88179A, `bcdDevice` 2.00 |
| Driver | in-tree `ax88179_178a` (`usbnet` + `mii`) |
| Kernel | `6.13.12-061312-generic` |
| USB link | SuperSpeed (`PHYSICAL_LINK_STATUS` 0x04) |

`GENERAL_STATUS` (MAC reg 0x03) read `0x06`. That is the AX88179A chip version. The 6.13 driver has no AX88179A-specific link path. Support for this chip is in an unmerged net-next series ("ax88179_178a: Add support for AX88179A-based chips", Birger Koblitz, 2026).

## What was changed

The dongle had been limited to 10baseT/Full:

```sh
sudo ethtool -s enx6c6e072f7dc4 advertise 0x002
```

It was then set to advertise 10Mb full and 100Mb full:

```sh
sudo ethtool -s enx6c6e072f7dc4 advertise 0x00a
```

`0x002` is 10baseT/Full. `0x008` is 100baseT/Full. `0x00a` is both. Auto-negotiation stayed on. This is not saved across unplug or reboot.

After that command, ethtool showed:

```text
Advertised link modes:  10baseT/Full
                        100baseT/Full
Link partner advertised link modes:  10baseT/Half 10baseT/Full
Speed: 10Mb/s
Duplex: Full
Auto-negotiation: on
Link detected: no
```

The other computer, cabled dongle-to-dongle, reported the link up and packets arriving. This host stayed `NO-CARRIER` with RX and TX counters at 0.

```text
enx6c6e072f7dc4: <NO-CARRIER,BROADCAST,MULTICAST,UP> state DOWN
RX: 0 packets, 0 bytes
TX: 0 packets, 0 bytes
```

Kernel log at 2026-10-04T13:34:00-07:00:

```text
ax88179_178a 4-2:1.0 enx6c6e072f7dc4: ax88179 - Link status is: 0
```

`ethtool` "Link detected" is `netif_carrier_ok()`, not the PHY link bit. Speed and duplex in that output come from the MII registers, which is why they can say 10Mb/full while "Link detected" is no.

## PHY and MAC dump

Read with a USB vendor control transfer on `/dev/bus/usb/004/006` while the driver was bound. PHY access is request `0x02`, `wValue` 3. MAC access is request `0x01`.

```text
PHY 00 BMCR              = 0x1140
PHY 01 BMSR              = 0x796d   (read twice, same value)
PHY 04 ANAR              = 0x0d41
PHY 05 ANLPAR            = 0x4461
PHY 06 ANER              = 0x0007
PHY 09 CTRL1000          = 0x0000
PHY 0a STAT1000          = 0x4000
PHY 0f                   = 0x2000
PHY 11 PHYSR             = 0x0000
PHY 12                   = 0x0048
PHY 13                   = 0x0000
PHY 1d CTRL_STATUS       = 0x0020
PHY 1e                   = 0x0000
PHY 1f page              = 0x0000
MAC 02 PHYSICAL_LINK     = 0x04
MAC 03 GENERAL_STATUS    = 0x06
MAC 0b RX_CTL            = 0x0298
MAC 22 MEDIUM_STATUS     = 0x0032
MAC 2e BULK_IN           = 07 4f 00 12 ff
```

### Decode

- `BMSR` 0x796d has link status (bit 2) and autoneg complete (bit 5). The second read matched the first, so this is not a latched-low link bit.
- `ANAR` 0x0d41 advertises selector, 10full, 100full, and symmetric pause. No half duplex and no 1000. That matches `advertise 0x00a`.
- `ANLPAR` 0x4461 is the partner: 10half, 10full, pause, and autoneg ack. The partner is not offering 100 or 1000.
- Common mode is 10baseT/Full.
- `CTRL1000` is 0, so this host is not advertising gigabit.
- Vendor `CTRL_STATUS` 0x1d is 0x0020. Speed field is bits 3:2 (`0x0c`). Those bits are 0, which the AX88179A PHY driver defines as 10 Mbps.
- `PHYSR` 0x11 is 0. The 6.13 `ax88179_link_reset()` treats bit 10 (`0x400`) as link up, and bits 15:14 as 1000/100. Zero means the driver takes the "no link" path and returns without turning the carrier on.
- `MEDIUM_STATUS` 0x0032 is full duplex plus RX and TX flow control. `AX_MEDIUM_RECEIVE_EN` (0x100) is clear, so the MAC is not accepting frames. Gigabit mode and the 100M bit are also clear, so the MAC speed select is 10.
- `RX_CTL` 0x0298 has the RX engine started (`AX_RX_CTL_START`), but medium-mode receive enable is what actually gates the path.
- `BULK_IN` `07 4f 00 12 ff` is the driver's gigabit SuperSpeed bulk-in preset (`AX88179_BULKIN_SIZE[0]`), not the 10M preset.

## Why this side looks down

The copper PHYs finished autoneg at 10Mb full duplex. The other host's driver reports that link and can receive. This host's `ax88179_178a` never marks the netdev up.

1. Carrier is driven only by interrupt URB bit 16 (`AX_INT_PPLS_LINK`). The last interrupt said link 0, and the interface is still `NO-CARRIER`.
2. On a reported link-up, `ax88179_link_reset()` reads PHY register 0x11 and gives up when bit 10 is clear. On this AX88179A that register is 0 even while `BMSR` says the link is up.
3. With carrier off, the stack sends nothing. `AX_MEDIUM_RECEIVE_EN` is off, so frames from the other host are not DMA'd and the RX counter stays 0.

The other side can still show link up, because that decision is made in its PHY. Frames it counts as received are not visible in this host's driver counters.

## After replug

Advertisement returns to the driver default. To get back to 10full + 100full only:

```sh
sudo ethtool -s enx6c6e072f7dc4 advertise 0x00a
sudo ethtool enx6c6e072f7dc4
ip -s link show enx6c6e072f7dc4
```

The partner in this test only offers 10half and 10full, so a successful negotiation is 10full even with 100full also advertised. 100full will not be selected unless the other dongle advertises it.

A replug does not fix the carrier bug. Expect `Link detected: no` again if the partner is up and ethtool still shows a resolved speed and duplex.

`dmesg` is restricted (`kernel.dmesg_restrict = 1`). The driver line is in the kernel journal:

```sh
sudo journalctl -k --since "10 min ago" | grep -i ax88179
```

The later Logic captures, after this replug, are in `data/t14s/`.
