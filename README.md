# libbacktrace-python

Python bindings for [libbacktrace](https://github.com/ianlancetaylor/libbacktrace) by Ian Lance Taylor.

This package provides Python access to native (C/C++) stack traces with DWARF symbol resolution on Linux and macOS.

## Installation

```bash
pip install libbacktrace-python
```

## Usage

```python
import libbacktrace

# Check if supported on this platform
if libbacktrace.supported():
    # Get current native stack trace
    frames = libbacktrace.get_backtrace()
    for frame in frames:
        print(f"  {frame.function} at {frame.filename}:{frame.lineno}")
```

## Platform Support

| Platform | Status | Symbol Resolution |
|----------|--------|-------------------|
| Linux | ✅ Full | DWARF via ELF |
| macOS | ✅ Full | DWARF via Mach-O |
| Windows | ❌ Not supported | - |

## Features

- Native C/C++ stack traces from Python
- DWARF debug symbol resolution (function names, file names, line numbers)
- Signal-safe operation (can be used in crash handlers)
- Minimal dependencies (just Python)

## Use Cases

- Crash reporting and diagnostics
- Debugging native extensions
- Performance profiling
- Error logging with full context

## Credits

This package provides Python bindings for **libbacktrace**, which is developed by 
[Ian Lance Taylor](https://github.com/ianlancetaylor) at Google and is part of the GCC project.

libbacktrace is licensed under the BSD 3-Clause License. See [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) for details.

## License

This package (the Python bindings and build configuration) is licensed under the Apache License 2.0.

The bundled libbacktrace library is licensed under the BSD 3-Clause License.
