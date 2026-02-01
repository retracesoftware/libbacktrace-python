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
    # Fault handler (signal handler for crashes)
    "enable_faulthandler",
    "disable_faulthandler",
    "faulthandler_enabled",
    "get_signals",
    "get_default_signals",
    # Signal name constants
    "SIGSEGV",
    "SIGABRT",
    "SIGFPE",
    "SIGBUS",
    "SIGILL",
    "SIGTRAP",
    "SIGSYS",
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


# =============================================================================
# Signal constants
# =============================================================================

SIGSEGV = "SIGSEGV"
SIGABRT = "SIGABRT"
SIGFPE = "SIGFPE"
SIGBUS = "SIGBUS"
SIGILL = "SIGILL"
SIGTRAP = "SIGTRAP"
SIGSYS = "SIGSYS"


# =============================================================================
# Fault handler - Signal handler for crashes (SIGSEGV, SIGABRT, etc.)
# =============================================================================

def get_signals() -> List[str]:
    """
    Get list of all available signal names that can be handled.
    
    Returns:
        List of signal name strings (e.g., ["SIGSEGV", "SIGABRT", ...])
    """
    if not _SUPPORTED:
        return []
    return _libbacktrace.get_signals()


def get_default_signals() -> List[str]:
    """
    Get list of default signals handled when none specified.
    
    Returns:
        List of signal name strings
    """
    if not _SUPPORTED:
        return []
    return _libbacktrace.get_default_signals()


def enable_faulthandler(
    signals: Optional[List[str]] = None,
    report_path: Optional[str] = None
) -> bool:
    """
    Enable native crash handler for specified signals.
    
    When a crash occurs, prints a native stack trace with symbols to stderr.
    Optionally saves the crash report to a file.
    
    This complements Python's built-in faulthandler module which only shows
    Python stack traces. For complete crash reports, enable both:
    
        import faulthandler
        faulthandler.enable()  # Python stack traces
        
        import libbacktrace
        libbacktrace.enable_faulthandler()  # Native stack traces
    
    Example with custom signals:
    
        import libbacktrace
        libbacktrace.enable_faulthandler(
            signals=[libbacktrace.SIGSEGV, libbacktrace.SIGABRT]
        )
    
    Args:
        signals: List of signal names to handle. Use get_signals() to see
                 available options. Default: SIGSEGV, SIGABRT, SIGFPE, SIGBUS
        report_path: Optional file path to save crash reports
        
    Returns:
        True if enabled successfully, False if not supported
    """
    if not _SUPPORTED:
        return False
    return _libbacktrace.enable_faulthandler(signals=signals, report_path=report_path)


def disable_faulthandler() -> bool:
    """
    Disable native crash handler and restore default signal handlers.
    
    Returns:
        True if disabled successfully, False if not supported
    """
    if not _SUPPORTED:
        return False
    return _libbacktrace.disable_faulthandler()


def faulthandler_enabled() -> bool:
    """
    Check if the native crash handler is currently enabled.
    
    Returns:
        True if crash handler is active
    """
    if not _SUPPORTED:
        return False
    return _libbacktrace.faulthandler_enabled()
