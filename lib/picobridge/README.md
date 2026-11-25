# Built in Pico bridge

This is [scopehal-pico-bridge](https://github.com/ngscopeclient/scopehal-pico-bridge) (ps6000d) as a library, used
by:

- ngscopeclient's `pico` transport (`lib/scopehal/SCPIPicoTransport.cpp`), which runs the bridge in process. The
  `PicoOscilloscope` driver is unchanged: it speaks the same protocol to this code as it does to a separate ps6000d
  over the `twinlan` transport.
- The standalone ps6000d socket server (`src/ps6000d`), for remote use with the `twinlan` transport as before.

It depends only on `lib/log` (not libscopehal), and is built only if CMake finds the Pico SDK headers.

Imported from:

- scopehal-pico-bridge 05842a2 (`src/ps6000d`)
- scpi-server-tools afcadd2 (`SCPIServer`, `BridgeSCPIServer`)

## Changes from upstream

Mechanical, so upstream patches can be ported by applying the same renames to them:

- Globals `g_foo` are members `m_foo` of `PicoSCPIServer` (except the constant `g_waveformTypes`). `limitChannels`
  and `picoGetUnitInfo` are `m_limitChannels` and `m_picoGetUnitInfo`.
- The free functions (`UpdateChannel`, `UpdateTrigger`, `Stop`, `StartInternal`, `StartCapture`, `EnableMsoPod`,
  `GenerateSquareWave`, `WaveformServerThread`, `CheckForACKs`, `InterpolateTriggerTime`, `Open2000`...`Open6000`)
  are `PicoSCPIServer` member functions.
- SDK calls go through the run time loaded function table: `ps6000aRunBlock(...)` is `m_api.ps6000aRunBlock(...)`.
  Every SDK function used must be listed in `PicoApiFunctions.h`.
- Everything is in namespace `picobridge`.

By hand:

- `main.cpp` is `PicoOpen.cpp`: `main()` is `PicoSCPIServer::Open()` (no argument parsing, sockets, signal handlers
  or `exit()`), the `OpenNNNN()` functions take the serial number to open and return `PICO_NOT_FOUND` if their SDK
  library isn't loaded, `CloseUnit()` is the close at the end of `main()`, and `EnumerateUnits()` is new.
- The `PicoSCPIServer` constructor is `StartClient()`, which also starts the waveform thread. The destructor is
  `StopClient()`, which stops the waveform thread first. The instrument stays open across clients, as in ps6000d;
  `~PicoSCPIServer()` closes it, and `Shutdown()` is `OnQuit()`.
- `SCPIServer` has no socket: `ProcessLine()` runs one command and `SendReply()` queues the reply for `PopReply()`.
- The waveform thread writes to `PicoDataPipe`s instead of the data socket, waits for ACKs instead of spinning, and
  its `LogFatal()` calls throw an exception that ends only the waveform thread (`LogFatal()` would end ngscopeclient).
- The waveform thread re-arms the trigger straight after `GetValues()`, before sending the data (ps6000d re-armed
  after sending), so the next capture runs while the client takes this one. The trigger phase and channel
  scale/offset are worked out before re-arming, since `StartCapture()` updates the "during arm" settings.
- `or` is `||` (MSVC).

## SDK

Only the Pico SDK headers are needed to build (CMake looks in the SDK's default install directory, or set
`PICO_SDK_INCLUDE_DIR`). The libraries (`ps6000a.dll`, `libps6000a.so`, ...) are loaded by `PicoApi` the first time
the `pico` transport is used or enumerated: from the ngscopeclient directory, then the SDK's default install
directory, then the standard search path. An API family whose library is missing, or lacks any function in
`PicoApiFunctions.h`, is skipped.
