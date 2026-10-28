# C Interactive Function Plotter

A lightweight, standalone desktop function plotting application written in pure C for Windows (Win32 API & GDI). Zero external dependencies required.

## Features

- **Custom Expression Input ($f(x)$)**:
  - Built-in recursive-descent math parser and evaluator with operator precedence and implicit multiplication (e.g. `2x`, `3sin(x)`).
  - Supports operators: `+`, `-`, `*`, `/`, `^` (power).
  - Supports functions: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `sqrt`, `cbrt`, `exp`, `ln`, `log10`, `abs`, `floor`, `ceil`.
  - Supports constants: `pi`, `e`.
- **Zooming Possibilities**:
  - **Zoom X-axis only**: UI buttons `[X +]` / `[X -]`, keyboard `X` / `Shift+X`, or `Shift + Mouse Wheel`.
  - **Zoom Y-axis only**: UI buttons `[Y +]` / `[Y -]`, keyboard `Y` / `Shift+Y`, or `Ctrl + Mouse Wheel`.
  - **Zoom Both at the same time**: UI buttons `[Both +]` / `[Both -]`, keyboard `+` / `-`, or standard `Mouse Wheel`.
- **Panning & Navigation**:
  - Click & drag with mouse on the graph to pan in any direction.
  - Arrow keys: Pan Left, Right, Up, Down.
  - `[Reset View]` button or `R` key to reset to default $[-10, 10]$ viewport.
- **Customizable Labels and Title**:
  - Edit fields for **Graph Title**, **X-Axis Label**, and **Y-Axis Label**.
  - Title renders centered at the top in bold typography.
  - X-axis label renders centered along the horizontal axis.
  - Y-axis label renders vertically ($90^\circ$ rotated) along the vertical axis.
  - Pressing `Enter` in any edit field immediately refreshes and plots the graph.
- **Visuals & Precision**:
  - Double-buffered GDI rendering (100% flicker-free).
  - Adaptive gridlines and auto-calculated "nice" tick intervals ($1, 2, 5 \times 10^k$).
  - Asymptote & jump discontinuity handling (avoids false vertical connecting lines across poles like in $\tan(x)$ or $1/x$).
  - Real-time cursor coordinates $(x, y)$ readout at the bottom bar.

## Building and Running

### With GCC (MinGW):
```bash
gcc -O2 -Wall -Wextra main.c -o plotter.exe -lgdi32 -luser32 -mwindows
./plotter.exe
```

Or simply run the provided `build.bat`.

