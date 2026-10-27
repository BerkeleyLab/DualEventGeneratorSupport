# Dual Event Generator — EPICS Support Module

This folder documents the **`DualEventGeneratorSupport`** EPICS module: the ASYN driver, record database, engineering IOC, Phoebus screens, and support scripts used to control and monitor the dual event generator.

| Document | What's inside |
|---|---|
| 📡 [EPICS Support](EPICSnotes.md) | Driver, database records and addresses, IOC startup, operator screens, scripts |

## Quick facts

| | |
|---|---|
| IOC configuration | `eventGeneratorConfigure(port, host, priority)` |
| Database | `eventGenerator.db` (from `eventGeneratorSup/Db/eventGenerator.substitutions`) |
| FPGA UDP ports | 58762 (commands), 58763 (sequencer status) |
| FPGA console | `scripts/console.py -a <address>` |

## Related projects

| Project | Documentation |
|---|---|
| **[dual-evg](https://gitlab.lbl.gov/alsu-instrumentation/dual-evg)** — event generator hardware and firmware: front panel, console commands, TFTP files, `DefaultSequence.csv`, building and flashing | [Overview ↗](https://gitlab.lbl.gov/alsu-instrumentation/dual-evg/-/blob/HEAD/documentation/README.md) · [Hardware ↗](https://gitlab.lbl.gov/alsu-instrumentation/dual-evg/-/blob/HEAD/documentation/Hardware.md) · [Firmware ↗](https://gitlab.lbl.gov/alsu-instrumentation/dual-evg/-/blob/HEAD/documentation/Firmware.md) · [Updating Firmware ↗](https://gitlab.lbl.gov/alsu-instrumentation/dual-evg/-/blob/HEAD/documentation/HowtoUpdateFirmware.md) |
| **[siocDualTiming](https://git.als.lbl.gov/accelerator-controls/epics/iocs/siocDualTiming)** — soft IOC that uses this module to build and send injection sequences | [Timing IOC ↗](https://git.als.lbl.gov/accelerator-controls/epics/iocs/siocDualTiming/-/blob/HEAD/documentation/TimingIOC.md) |
