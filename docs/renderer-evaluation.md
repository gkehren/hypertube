# Slint renderer evaluation

Hypertube keeps Slint's software renderer as the production default. The
optional FemtoVG build exists to make renderer changes measurable and does not
alter normal builds or packages.

## Method

`slint-renderer-comparison` runs the production Slint shell at 1280×760. Each
backend receives 12 warm-up cycles and 120 measured redraw cycles at a 16 ms
cadence while the active view and theme alternate. Separate processes provide
comparable wall time, process CPU time, peak resident memory, cycle latency,
and event-loop stability. Slint's `take_snapshot()` is deliberately not used
for the comparison because GPU backends do not expose an equivalent pixel
buffer through that API.

## Windows reference run

The reference measurement below was collected on 2026-08-06 from an MSVC
Release build. It is evidence for this development machine, not a universal
hardware benchmark.

| Renderer | Stable cycles | Mean cycle | p95 cycle | CPU time | Peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: |
| Software | 132/132 | 16.31 ms | 17.08 ms | 2.17 s | 49.7 MiB |
| FemtoVG | 132/132 | 16.18 ms | 16.84 ms | 2.54 s | 161.1 MiB |

The 16 ms scheduler bounds cycle latency, so the similar latency figures do not
show a rendering-speed advantage. FemtoVG consumed more CPU time and roughly
three times the peak resident memory in this run. There is therefore no
measured basis for changing the production renderer, and software remains the
default.

Headless Linux CI repeats the stability comparison through Mesa and uploads
the raw JSON reports. A future renderer change requires representative native
GPU measurements on Windows, Linux X11/Wayland, and macOS, plus a documented
CPU, memory, and stability improvement.
