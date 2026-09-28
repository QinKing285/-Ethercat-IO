# EtherCAT CiA402 GUI Integration

This directory contains the SSC 5.13 CiA402 stack used by the full-function
GUI firmware. It uses the HPM6E80 internal ESC and the existing GUI
`motor_foc` implementation; the EtherCAT sample motor source is not linked.

## Device identity

- Vendor ID: `0x0048504D`
- Product code: `0x00000003`
- Revision: `0x00000002`
- Physical ports: `YY`
- Supported drive mode: CSV (`0x6060 = 9`)

Import `SSC/ESI/ECAT_CIA402.xml` into the EtherCAT master. Revision 2 also
forces the firmware's Flash EEPROM emulation to refresh older revision-1
content at the next boot.

## PDO and control

The default CSV mapping uses RxPDO `0x1602` and TxPDO `0x1A02`. Use the
standard controlword sequence `0x0006`, `0x0007`, `0x000F`. Configure DC
Sync0 before enabling the axis.

The SDK sample velocity scale is preserved: one `0x60FF` unit is about
`6.5532 RPM`. The shared FOC currently accepts forward targets from 0 to
2400 RPM. CSP and reverse-speed control are not exposed by this adapter.
