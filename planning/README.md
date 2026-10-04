# Wire speed

This Pico 2W path stays at 10 Mbit/s. The chip has no Ethernet PHY. TX is two GPIOs and RX is one comparator pin, clocked from the RP2350 PIO.

`ping` and `echo` run the system clock at 120 MHz. The transmitter emits one Manchester half-symbol per PIO instruction at 20 MHz, so a bit is 100 ns. The receiver samples the TLV3501 at 60 MHz, three samples per half-symbol. Both rates divide 120 MHz evenly. That is 10BASE-T.

The RP2350 is rated for 150 MHz, and the PIO cannot run faster than that clock. Keeping three samples on each half-symbol means the sample clock is six times the bit rate, so 150 MHz tops out at 25 Mbit/s. 100 Mbit/s is already past that, before the line code changes.

100 Mbit/s Fast Ethernet is 4B/5B and MLT-3 at 125 Mbaud, one symbol every 8 ns, with three analog levels. Three samples per symbol would be 375 MHz. The TLV3501 output is only high or low, so a third level never arrives at GP18.

1000 Mbit/s is 125 Mbaud PAM-5 on four pairs at once, with the PHY cancelling its own echo. Two GPIO pins and one comparator cannot make or see that signal.
