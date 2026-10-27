# Dual Event Generator Support

EPICS support module and engineering IOC for the ALS-U dual event generator. It provides an ASYN driver that talks to the event generator FPGA over UDP, a record database, and Phoebus engineering screens.

## Repository layout

| Path | Contents |
|---|---|
| `eventGeneratorSup` | ASYN driver (`src`) and database templates (`Db`) |
| `eventGeneratorApp` | Engineering IOC application and `eventGenerator.iocsh` |
| `iocBoot/iocDualEventGenerator` | IOC startup script and run scripts |
| `op/bob` | Phoebus engineering screens |
| `scripts` | `console.py`, `Latencies.py`, and other utilities |
| `documentation` | User documentation |

## Building

Requires EPICS base with the asyn, std, autosave, recsync, retools, caPutLog, iocStats, pvxs, and siteApps modules. Set `EPICS_BASE` (and `MODULES` if needed) in `configure/RELEASE.local`, then:

```sh
make
```

## Running the engineering IOC

```sh
cd iocBoot/iocDualEventGenerator
./st.cmd
```

The event generator address comes from the `IP` environment variable. `./run` and `./rund` start the IOC in a `screen` session (foreground or detached), and `./attach` reconnects to it.

## Documentation

📖 **[documentation/README.md](documentation/README.md)** — driver, records and addresses, IOC startup, screens, and scripts.

## Related projects

| Project | Contents |
|---|---|
| [dual-evg](https://gitlab.lbl.gov/alsu-instrumentation/dual-evg) | Event generator hardware, gateware, and µBlaze software |
| [siocDualTiming](https://git.als.lbl.gov/accelerator-controls/epics/iocs/siocDualTiming) | Soft IOC that uses this module to build and send injection sequences |
