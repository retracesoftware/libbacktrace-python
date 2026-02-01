"""
libbacktrace-python - Native stack traces with DWARF symbol resolution

Python bindings for libbacktrace by Ian Lance Taylor.
https://github.com/ianlancetaylor/libbacktrace
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from typing import List, Optional

__all__ = [
    "supported",
    "import_error",
    "get_backtrace",
    "print_backtrace",
    "create_state",
    "BacktraceFrame",
    "BacktraceState",
]

# Try to import the native extension
_IMPORT_ERROR: Optional[str] = None
try:
    from . import _libbacktrace
    _SUPPORTED = True
except Exception as e:
    _libbacktrace = None  # type: ignore
    _SUPPORTED = False
    _IMPORT_ERROR = f"{type(e).__name__}: {e}"


@dataclass
class BacktraceFrame:
    """A single frame in a native stack trace."""
    pc: int  # Program counter / instruction pointer
    function: Optional[str]  # Function name (None if unknown)
    filename: Optional[str]  # Source file path (None if unknown)
    lineno: int  # Line number (0 if unknown)
    
    def __str__(self) -> str:
        func = self.function or "??"
        if self.filename:
            return f"{func} at {self.filename}:{self.lineno}"
        return f"{func} at 0x{self.pc:x}"


class BacktraceState:
    """
    Opaque state object for libbacktrace.
    
    Creating a state allows libbacktrace to cache symbol information,
    making subsequent backtrace calls faster.
    """
    
    def __init__(self, filename: Optional[str] = None, threaded: bool = True):
        """
        Create a new backtrace state.
        
        Args:
            filename: Path to executable (None for current process)
            threaded: Whether to support multi-threaded access
        """
        if not _SUPPORTED:
            raise RuntimeError("libbacktrace not supported on this platform")
        self._state = _libbacktrace.create_state(filename, threaded)
    
    def get_backtrace(self, skip: int = 0) -> List[BacktraceFrame]:
        """
        Get the current native stack trace.
        
        Args:
            skip: Number of frames to skip from the top
            
        Returns:
            List of BacktraceFrame objects
        """
        raw_frames = _libbacktrace.backtrace_full(self._state, skip + 1)
        return [
            BacktraceFrame(pc=f[0], function=f[1], filename=f[2], lineno=f[3])
            for f in raw_frames
        ]


# Default global state (lazily initialized)
_default_state: Optional[BacktraceState] = None


def _get_default_state() -> BacktraceState:
    """Get or create the default backtrace state."""
    global _default_state
    if _default_state is None:
        _default_state = BacktraceState()
    return _default_state


def supported() -> bool:
    """
    Check if libbacktrace is supported on this platform.
    
    Returns:
        True if native backtraces are available
    """
    return _SUPPORTED


def import_error() -> Optional[str]:
    """
    Get the error message if the native extension failed to import.
    
    Returns:
        Error message string, or None if import succeeded
    """
    return _IMPORT_ERROR


def create_state(filename: Optional[str] = None, threaded: bool = True) -> BacktraceState:
    """
    Create a new backtrace state for caching symbol information.
    
    Args:
        filename: Path to executable (None for current process)
        threaded: Whether to support multi-threaded access
        
    Returns:
        BacktraceState object
    """
    return BacktraceState(filename, threaded)


def get_backtrace(skip: int = 0) -> List[BacktraceFrame]:
    """
    Get the current native stack trace.
    
    Uses a default shared state. For better performance in hot paths,
    create your own BacktraceState and reuse it.
    
    Args:
        skip: Number of frames to skip from the top
        
    Returns:
        List of BacktraceFrame objects, or empty list if not supported
    """
    if not _SUPPORTED:
        return []
    return _get_default_state().get_backtrace(skip + 1)


def print_backtrace(skip: int = 0, file=None) -> None:
    """
    Print the current native stack trace.
    
    Args:
        skip: Number of frames to skip from the top
        file: File to print to (default: sys.stderr)
    """
    if file is None:
        file = sys.stderr
    
    frames = get_backtrace(skip + 1)
    if not frames:
        print("  (native backtrace not available)", file=file)
        return
    
    for i, frame in enumerate(frames):
        print(f"  #{i} {frame}", file=file)
