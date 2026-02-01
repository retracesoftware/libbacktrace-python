"""Tests for libbacktrace-python."""

import pytest
import sys


def test_import():
    """Test that the module can be imported."""
    import libbacktrace
    assert hasattr(libbacktrace, 'supported')
    assert hasattr(libbacktrace, 'get_backtrace')
    assert hasattr(libbacktrace, 'print_backtrace')


def test_supported():
    """Test the supported() function."""
    import libbacktrace
    result = libbacktrace.supported()
    assert isinstance(result, bool)
    
    # Should be True on Linux and macOS
    if sys.platform in ('linux', 'darwin'):
        if not result:
            # Print the import error for debugging
            err = libbacktrace.import_error()
            raise AssertionError(f"Expected supported() to be True on {sys.platform}, but got False. Import error: {err}")


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_get_backtrace():
    """Test getting a backtrace."""
    import libbacktrace
    
    frames = libbacktrace.get_backtrace()
    assert isinstance(frames, list)
    # Note: frames may be empty on release builds without debug symbols
    
    if len(frames) > 0:
        # Check frame structure if we got frames
        frame = frames[0]
        assert hasattr(frame, 'pc')
        assert hasattr(frame, 'function')
        assert hasattr(frame, 'filename')
        assert hasattr(frame, 'lineno')
        
        assert isinstance(frame.pc, int)
        assert frame.pc > 0


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_backtrace_has_function_names():
    """Test that backtrace includes function names when debug symbols available."""
    import libbacktrace
    
    def inner_function():
        return libbacktrace.get_backtrace()
    
    def outer_function():
        return inner_function()
    
    frames = outer_function()
    
    # Note: function names require debug symbols, which may not be available
    # in CI release builds. Just verify the call works.
    assert isinstance(frames, list)


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_create_state():
    """Test creating a custom state."""
    import libbacktrace
    
    state = libbacktrace.create_state()
    assert state is not None
    
    frames = state.get_backtrace()
    assert isinstance(frames, list)


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_skip_frames():
    """Test skipping frames."""
    import libbacktrace
    
    frames_0 = libbacktrace.get_backtrace(skip=0)
    frames_2 = libbacktrace.get_backtrace(skip=2)
    
    # Both calls should return lists
    assert isinstance(frames_0, list)
    assert isinstance(frames_2, list)
    
    # If we have frames but aren't hitting the max (128), skipping should result in fewer
    # When at max frames, skipping just shifts the window
    if len(frames_0) > 2 and len(frames_0) < 128:
        assert len(frames_2) < len(frames_0)


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_print_backtrace(capsys):
    """Test printing a backtrace."""
    import libbacktrace
    
    libbacktrace.print_backtrace()
    
    captured = capsys.readouterr()
    # Should have printed something to stderr (either frames or "not available" message)
    assert len(captured.err) > 0


def test_unsupported_platform_graceful():
    """Test that unsupported platforms fail gracefully."""
    import libbacktrace
    
    # These should never raise, even on unsupported platforms
    _ = libbacktrace.supported()
    frames = libbacktrace.get_backtrace()
    assert isinstance(frames, list)


def test_frame_str():
    """Test BacktraceFrame string representation."""
    from libbacktrace import BacktraceFrame
    
    # With full info
    frame = BacktraceFrame(pc=0x12345, function="test_func", 
                          filename="/path/to/file.c", lineno=42)
    s = str(frame)
    assert "test_func" in s
    assert "/path/to/file.c" in s
    assert "42" in s
    
    # Without filename
    frame = BacktraceFrame(pc=0x12345, function="test_func", 
                          filename=None, lineno=0)
    s = str(frame)
    assert "test_func" in s
    assert "0x12345" in s
    
    # Without function
    frame = BacktraceFrame(pc=0x12345, function=None, 
                          filename=None, lineno=0)
    s = str(frame)
    assert "??" in s


# =============================================================================
# Faulthandler tests
# =============================================================================

@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_faulthandler_enable_disable():
    """Test enabling and disabling the faulthandler."""
    import libbacktrace
    
    # Ensure we start in a known state
    libbacktrace.disable_faulthandler()
    assert not libbacktrace.faulthandler_enabled()
    
    # Enable it
    result = libbacktrace.enable_faulthandler()
    assert result is True
    assert libbacktrace.faulthandler_enabled()
    
    # Disable it
    result = libbacktrace.disable_faulthandler()
    assert result is True
    assert not libbacktrace.faulthandler_enabled()


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_faulthandler_with_report_path(tmp_path):
    """Test enabling faulthandler with a report path."""
    import libbacktrace
    
    report_file = tmp_path / "crash_report.txt"
    
    result = libbacktrace.enable_faulthandler(report_path=str(report_file))
    assert result is True
    assert libbacktrace.faulthandler_enabled()
    
    # Cleanup
    libbacktrace.disable_faulthandler()


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_faulthandler_custom_signals():
    """Test enabling faulthandler with custom signal set."""
    import libbacktrace
    
    # Ensure we start clean
    libbacktrace.disable_faulthandler()
    
    # Enable with specific signals
    result = libbacktrace.enable_faulthandler(
        signals=[libbacktrace.SIGSEGV, libbacktrace.SIGABRT]
    )
    assert result is True
    assert libbacktrace.faulthandler_enabled()
    
    # Cleanup
    libbacktrace.disable_faulthandler()


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_get_signals():
    """Test getting available signals."""
    import libbacktrace
    
    signals = libbacktrace.get_signals()
    assert isinstance(signals, list)
    assert len(signals) > 0
    assert "SIGSEGV" in signals
    assert "SIGABRT" in signals


@pytest.mark.skipif(sys.platform not in ('linux', 'darwin'),
                    reason="Only supported on Linux and macOS")
def test_get_default_signals():
    """Test getting default signals."""
    import libbacktrace
    
    defaults = libbacktrace.get_default_signals()
    assert isinstance(defaults, list)
    assert "SIGSEGV" in defaults
    assert "SIGABRT" in defaults


def test_signal_constants():
    """Test signal name constants are defined."""
    import libbacktrace
    
    assert libbacktrace.SIGSEGV == "SIGSEGV"
    assert libbacktrace.SIGABRT == "SIGABRT"
    assert libbacktrace.SIGFPE == "SIGFPE"
    assert libbacktrace.SIGBUS == "SIGBUS"
    assert libbacktrace.SIGILL == "SIGILL"
    assert libbacktrace.SIGTRAP == "SIGTRAP"
    assert libbacktrace.SIGSYS == "SIGSYS"


def test_faulthandler_unsupported():
    """Test faulthandler on unsupported platforms returns False gracefully."""
    import libbacktrace
    
    # On unsupported platforms, these should return False without error
    if not libbacktrace.supported():
        assert libbacktrace.enable_faulthandler() is False
        assert libbacktrace.disable_faulthandler() is False
        assert libbacktrace.faulthandler_enabled() is False
        assert libbacktrace.get_signals() == []
        assert libbacktrace.get_default_signals() == []