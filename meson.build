# meson.build - libbacktrace-python build configuration

project(
  'libbacktrace-python',
  'c',
  version: run_command('python', '-m', 'setuptools_scm', check: true, capture: true).stdout().strip(),
  default_options: ['c_std=c11', 'warning_level=1'])

py = import('python').find_installation(pure: false)

# Platform detection
is_linux = host_machine.system() == 'linux'
is_macos = host_machine.system() == 'darwin'
is_windows = host_machine.system() == 'windows'

# Only build on supported platforms
if not (is_linux or is_macos)
  error('libbacktrace-python only supports Linux and macOS')
endif

# libbacktrace source directory
libbacktrace_dir = 'vendor/libbacktrace'

# Common libbacktrace sources
libbacktrace_common = files(
  libbacktrace_dir / 'atomic.c',
  libbacktrace_dir / 'backtrace.c',
  libbacktrace_dir / 'dwarf.c',
  libbacktrace_dir / 'fileline.c',
  libbacktrace_dir / 'posix.c',
  libbacktrace_dir / 'print.c',
  libbacktrace_dir / 'simple.c',
  libbacktrace_dir / 'sort.c',
  libbacktrace_dir / 'state.c',
)

# Platform-specific sources and compiler args
c_args = []

if is_linux
  libbacktrace_platform = files(
    libbacktrace_dir / 'elf.c',
    libbacktrace_dir / 'mmap.c',
    libbacktrace_dir / 'mmapio.c',
  )
  # GNU extensions needed for MAP_ANONYMOUS, dl_iterate_phdr, etc.
  c_args += ['-D_GNU_SOURCE']
elif is_macos
  libbacktrace_platform = files(
    libbacktrace_dir / 'macho.c',
    libbacktrace_dir / 'alloc.c',
    libbacktrace_dir / 'read.c',  # Provides backtrace_get_view for non-mmap systems
  )
endif

# Our Python extension source
extension_sources = files('src/libbacktrace/_libbacktrace.c')

# All sources combined
all_sources = libbacktrace_common + libbacktrace_platform + extension_sources

# Include directories
# Note: src/libbacktrace contains our config.h and backtrace-supported.h
# These must be searched BEFORE vendor/libbacktrace to override the .in templates
inc_dirs = include_directories('src/libbacktrace', libbacktrace_dir)

# Build the extension module
py.extension_module(
  '_libbacktrace',
  sources: all_sources,
  include_directories: inc_dirs,
  c_args: c_args,
  install: true,
  subdir: 'libbacktrace',
)

# Install Python package files
py.install_sources(
  'src/libbacktrace/__init__.py',
  subdir: 'libbacktrace',
)
