# Protocol Notes

`PGN-Verzeichnis.yaml` is the current AgOpenGPS PGN directory used as the
reference for this firmware.

The first implementation pass uses it primarily for:

- PGN 250: From Autosteer 2
- PGN 253: Steer Status Out
- PGN 254: Steer Data In
- PGN 251: Steer Config In
- PGN 252: Steer Settings In

The YAML is checked in as documentation/reference only. No code generation is
performed from it yet.

## Hardware References

- [BNO080/BNO085 Datasheet](../BNO080_085-Datasheet.pdf): reference for BNO085
  SPI timing, INT signalling, SH-2 packet handling, and sensor report rates.
