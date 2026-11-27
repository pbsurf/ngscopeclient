# ngscopeclient and scopehal-apps

This is a fork of [ngscopeclient/scopehal-apps](https://github.com/ngscopeclient/scopehal-apps) (upstream [user manual](https://www.ngscopeclient.org/manual/ngscopeclient-manual.html)).  Due to the use of AI here, forks of this repo should not submit pull requests to the original ngscopeclient.

Our forks of scopehal, logtools, imgui, imgui-node-editor and the manual text are combined into this one repository, in the directories where their submodules were. `scripts/sync-upstream.sh` merges in upstream changes.

## Downloads

Windows and Linux x64 builds of the latest master are at the
[nightly prerelease](https://github.com/pbsurf/ngscopeclient/releases/tag/nightly), updated by GitHub Actions on
every push.

**Windows** ([direct link](https://github.com/pbsurf/ngscopeclient/releases/download/nightly/ngscopeclient-windows-x64.zip)):
unzip it, keeping the files together, and run `ngscopeclient.exe`.

* The build isn't signed, so Windows SmartScreen may warn on the first start: More info, Run anyway.
* PicoScopes (pico transport) need the [PicoSDK](https://www.picotech.com/downloads) (64-bit) installed: its
  libraries are loaded from `C:\Program Files\Pico Technology\SDK\lib`.

**Linux** ([direct link](https://github.com/pbsurf/ngscopeclient/releases/download/nightly/ngscopeclient-linux-x64.tar.gz)):
runs on distributions with glibc 2.28 or later (Debian 10+, Ubuntu 20.04+, RHEL/Rocky 8+, current Fedora and Arch).
Extract it with `tar xf ngscopeclient-linux-x64.tar.gz`, keeping the files together, and run
`ngscopeclient/ngscopeclient`.

* It needs a Vulkan driver: `mesa-vulkan-drivers` (Debian/Ubuntu) or `mesa-vulkan-drivers`/`vulkan-radeon`/`vulkan-intel`
  (Fedora/Arch) for Intel and AMD GPUs, or NVIDIA's driver.
* File dialogs use the desktop portal (`xdg-desktop-portal`), which most desktops install.
* PicoScopes (pico transport) need Pico's driver packages, such as `libps6000a`, from
  [Pico's Linux repository](https://www.picotech.com/downloads/linux). They install to `/opt/picoscope/lib`, where
  ngscopeclient looks for them, along with the udev rules for USB access.
* USB instruments other than PicoScopes may need udev rules to be used without root. A Pluto works without them
  over its USB network (`ip:192.168.2.1`).

## New features

* HTTP Export filter: serves scalar values and analog waveforms (JSON, CSV or NumPy) over HTTP, and lets clients trigger acquisitions
* Up/Down in numeric boxes steps the digit left of the cursor
* Option to pause acquisition while the window is minimized; while minimized, waveforms aren't rendered and come in no faster than when visible

### SDR: AD936x radios (ADALM-PLUTO etc.)

* SDR support for AD936x radios via optional [libiio](https://github.com/analogdevicesinc/libiio): RX frequency, bandwidth, sample rate and gain
* SDR transmit: DDS tones with frequency/amplitude, TX gain and TX LO in the stream browser
* SDR sweep: spans wider than one capture sweep the LO, and the new Spectrum Stitch filter joins the captures into one spectrum
* SDR level correction: receive gain taken out of the I/Q samples, external gain, and a calibration file (text or JSON, which can also be pasted in place of the path)
* Optional RSSI scalar stream, Sweep toggle with a settable sweep step, and the transmit LO can be driven from the filter graph
* Transmit levels: a per-path TX calibration and external gain (in the new properties dialog for transmit paths) give each tone a Level in dBm, which can be set directly; the gain and the other tone's amplitude follow so nothing clips
* The RX LO tunes down to 46.875 MHz, below the Linux driver's 70 MHz limit
* Receive gain replaces attenuation in the stream browser and channel properties for radios with gain control
* The IIO mock radio loops transmit back to receive, with realistic signal levels and noise, for testing without hardware
* Fixed SDR sweeps putting signals in the wrong place in stitched spectra
* Fixed SDR channels losing stream types on session load, and sessions saved before a driver added a stream (such as RSSI) loading a complex channel's center frequency as an analog stream
* Rough [ADALM-PLUTO calibrations](#adalm-pluto-calibration-very-rough) below

### PicoScope

* PicoScope support built in: the `pico` transport runs [scopehal-pico-bridge](https://github.com/ngscopeclient/scopehal-pico-bridge) inside ngscopeclient, so no separate ps6000d process is needed (path: the scope's serial number, or empty for the first one found).
* Add double-buffering to improve performance
* The PicoSDK libraries are loaded at run time if they're installed, so ngscopeclient runs normally without them, and only the SDK headers are needed to build
* The standalone ps6000d server is still built from the same code with `-DBUILD_PS6000D=ON`, for use over the twinlan transport

### Spectrum analysis

* Complex FFT filter for I/Q data
* FFT Length setting for FFT and Complex FFT: shorter FFTs averaged over the input (Welch's method), for a smoother noise floor and much less downstream work (Spectrum Stitch, peak search, rendering) at 1M points
* The channel tooltip of a spectrum shows its RBW (resolution bandwidth, including the window's noise bandwidth), also through Spectrum Stitch and Waterfall
* Detector setting (Normal/Peak/Average) for drawing FFT and Spectrum Stitch
* Peak Select filter: outputs the frequency, level and FWHM of one detected peak as scalars, chosen by rank, within a range of a target, or nearest to it
* Peak Table window for FFT, Spectrum Stitch and Peaks filters (plot context menu or Window menu): lists the labeled peaks and the next few, sets how many are labeled and the Peak Window, highlights a peak on the plot on hover and centers on it on click, and copies as CSV (right click)
* Markers at the plot edge for spectrum peaks scrolled out of view, and FFT peak labels that hold still on noisy spectra
* Peak markers show frequency to a tenth of a bin, and FWHM as an upper bound at the two-bin minimum
* Faster peak search when the Peak Window spans many bins (8.0 to 2.1 ms on a 1M point spectrum)
* Fixed the FFT's bin width, which put frequencies low by up to one bin near Nyquist (a whole bin at short lengths)
* Fixed FFT peak detection ignoring the Peak Window (it reported every local maximum as a peak)
* Fixed spectrograms when the capture depth changes but the FFT length doesn't, an FFT crash on an empty input, and autoscale ignoring the FFT detector setting
* The spectrum channel tooltip shows the start frequency and bin spacing (it showed a meaningless sample rate)

### Plots and navigation

* Plot interaction: drag to pan, Ctrl+drag box zoom, double click an axis to autofit, pinch zoom and touch improvements
* Fixed divisions grid mode, like a scope's graticule; optional vertical grid, dashed grid lines and compact axes
* Ctrl+mouse wheel zooms in 1-2-5 steps per division, and dragging the Y axis offset snaps to 1/5 division (hold Shift for continuous)
* Per-stream display colors, such as separate colors for I and Q
* CLIP indicator next to a waveform's button when it, or anything feeding it through filters, is clipping
* Timeline menu: Go to t = 0, Go to trigger, Zoom to Cursors
* The horizontal view uses double precision, so it can span more than 2.5 hours and cursors work far from t = 0

### Touch input

* Windowing and input ported from GLFW to SDL2, enabling touch input support
* Touchscreen: pinch zoom and pan in the filter graph, tap one port then another to connect them, double tap and drag to zoom a plot, two finger scrolling in windows

### Sessions and instruments

* Sessions can be saved as a single `.scopearchive` (zip) file, or as a `.scopeconfig` without waveform data; option to save the configuration on exit
* Preference to open the most recent session file at startup (ask, reconnect or offline)
* Recent Instruments section in Manage Instruments
* Instruments that can't be connected and have no offline driver (SDRs, function generators, etc.) are named in a popup, and the save on exit is skipped while they're missing, so they aren't dropped from the session file

### Windows and Linux builds and CI

* Native Windows build with MSVC (vcpkg manifest and CMake presets), including the AVX code paths
* Portable Linux build (manylinux_2_28 container, dependencies from the same vcpkg manifest), runs on glibc 2.28+
* GitHub Actions builds Windows and Linux on every push to master, with the pico transport, runs the unit tests on
  Linux (on Mesa's software Vulkan), and publishes both as the [nightly prerelease](#downloads)

## Minor features and bug fixes

* Precise axis labels and cursor positions when zoomed far in
* Numeric boxes keep the typed value when the instrument rounds it, and update while being stepped if it changes
* A bare number typed in a numeric box keeps the unit and prefix shown before ("2" in a "1 MHz" box is 2 MHz)
* `--reconnect` and `--offline` options to skip the reconnect prompt when opening a session from the command line
* Better responsiveness in event-driven (power saving) mode
* Simulated function generator (demofuncgen) for demos and UI testing
* Fixed unit parsing that read a unit's first letter as a prefix ("5 mV" in millivolts, "2 m" in meters)
* Fixed value formatting dropping digits ("1.0004 GHz" shown as "1 GHz")
* Fixed staircase function generator shapes loading as sine
* Fixed empty saved font paths overriding the default fonts
* Build: slimmer `scopehal.h` for in-tree builds, so changing a rarely used header rebuilds far fewer files
* Build: [mold](https://github.com/rui314/mold) linker used when installed
* Values limited by the instrument while stepping keep their prefix and step size; clicking a drag field or slider lets the arrow keys step it
* Unit formatting uses all SI prefixes for small values and no prefix for zero, and several parsing bugs are fixed
* Window startup mode defaults to Last State, so the window comes back where it was
* Uncapped framerate toggle in Performance Metrics, for benchmarking
* The demo oscilloscope has an edge trigger and trigger offset
* The Trend filter can span more than 2.5 hours
* Fixed races in the filter graph executor that skipped filter runs or hung on exit
* Fixed a crash on exit after acquiring with two independently triggered scopes
* Fixed Scalar Stairstep taking every missed step after a stall instead of resyncing
* Fixed the filter graph being drawn at 1x on HiDPI displays
* Fixed 100% CPU use while minimized in Performance mode
* Fixed the `-L` line buffered log file option on Windows
* Build: changing `Unit.h` or `ngscopeclient.h` rebuilds fewer files
* Sessions remember whether the window was maximized, instead of moving a full-screen sized window off the screen
* Fixed a docked window's saved tab not being selected when the session layout loads
* `--perf-log <n>` logs the waveform rate and the filter graph, render and per-filter times every n waveforms, for benchmarking from scripts

## ADALM-PLUTO calibration (very rough)

Measured on one Pluto, so only a starting point for others: expect a few dB of error, more below 100 MHz. Paste the
JSON into the calibration box of the channel properties (RX) or the transmit path's properties (TX: double click it
in the stream browser).

* RX1, the gain in dB (sine into 50 Ω, Pluto reading vs scope):
  `[[60e6, 10.5], [80e6, 11.2], [130e6, 11.8], [400e6, 12.0]]`
  The level is corrected at the LO only, so signals far from it in a wide capture read low below about 60 MHz.
* TX1, dBm of one tone at 100% amplitude and 0 dB gain (loopback through a 20 dB pad into the calibrated RX1):
  `[[60e6, -2.0], [100e6, 1.9], [190e6, 6.9], [290e6, 8.5], [390e6, 9.1], [490e6, 9.6]]`

Below 70 MHz the Pluto's mixer harmonics aren't filtered, so use a low pass filter there for anything but rough
levels.

## TODO

- test gnu radio blocks


## CI

This fork builds with GitHub Actions (`.github/workflows`); see [Downloads](#downloads). Upstream ngscopeclient
moved to its own CI system with GPU and hardware-in-loop testing:
[dashboard](https://dashboard.ngscopeclient.org/index.php?project=ngscopeclient),
[binaries](https://dl1.ngscopeclient.org/ngscopeclient-ci/).

## Building

Clone with the submodules:

```
git clone --recursive https://github.com/pbsurf/ngscopeclient.git
```

**Windows (MSVC):** Visual Studio 2022 or its Build Tools with the C++ workload, and vcpkg with `VCPKG_ROOT` set
(`msvc\install-buildtools.ps1` installs all of it; details in [devdoc/WindowsMSVC.md](devdoc/WindowsMSVC.md)). From
an x64 Developer Command Prompt:

```
cmake --preset windows-msvc
cmake --build --preset windows-msvc
```

The first configure builds the dependencies with vcpkg (`msvc\vcpkg.json`), which takes a while. The program is in
`build\windows-msvc\src\ngscopeclient`. Other presets: `windows-msvc-debug`, and `windows-msvc-tests` for the unit
tests (`ctest --preset windows-msvc-tests`; they need a Vulkan GPU).

**Linux (Debian 12+, Ubuntu 23.04+):** install the dependencies, with Vulkan from the distribution's packages:

```
sudo apt install build-essential git cmake ninja-build pkgconf lsb-release \
  libsdl2-dev libgtk-3-dev libwayland-dev libsigc++-2.0-dev libyaml-cpp-dev libhidapi-dev libpng-dev zlib1g-dev \
  libvulkan-dev glslang-dev glslang-tools spirv-tools glslc
```

Optional: `libiio-dev` for the AD936x SDRs (libiio 0.x; 1.x isn't supported), and `catch2 libfftw3-dev` for the
unit tests (`-DBUILD_TESTING=ON`). Then:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The program is `build/src/ngscopeclient/ngscopeclient`. Older distributions (Ubuntu 22.04) need the
[Vulkan SDK](https://vulkan.lunarg.com/sdk/home) instead of the Vulkan packages: source its `setup-env.sh` before
running cmake.

**Linux, portable (as CI builds it):** `scripts/ci/linux-container.sh` runs `scripts/ci/build-linux.sh` in the
manylinux_2_28 container with podman or docker, and leaves `dist/ngscopeclient-linux-x64.tar.gz`. The first run
builds the dependencies with vcpkg, which takes a while; vcpkg and its caches are kept in
`~/.cache/ngscopeclient-ci`. Steps can be run on their own (`scripts/ci/linux-container.sh build package`, or
`shell` for a shell in the container). The unit tests run outside the container, from the test bundle the package
step also makes (`dist/ngscopeclient-linux-x64-tests.tar.gz`; see the top of `scripts/ci/linux-container.sh`).

**PicoScope (pico transport):** built when CMake finds the PicoSDK headers, in the SDK's default place
(`C:\Program Files\Pico Technology\SDK\inc`, or `/opt/picoscope/include` from Pico's Linux packages such as
`libps6000a`) or in `-DPICO_SDK_INCLUDE_DIR=<dir>`.
`python scripts/ci/fetch-pico-headers.py <dir>` downloads just the headers from Pico's package repository.
`-DREQUIRE_PICO=ON` makes missing headers an error, and `-DBUILD_PS6000D=ON` also builds the standalone ps6000d
server.

## Special comments

The following standard comments are used throughout the code to indicate things that could use attention, but are
not worthy of being tracked as a GitHub issue yet.

* `//TODO`: unimplemented feature, potential optimization point, etc.
* `//FIXME`: known minor problem, temporary workaround, or something that needs to be reworked later
* `//FIXME-CXX20`: places where use of C++ 20 features would simplify the code, but nothing can be done as long as we are targeting platforms which only support C++ 17
