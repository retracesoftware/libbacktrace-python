/*
 * config.h - Configuration for libbacktrace
 * 
 * Minimal configuration for building libbacktrace as part of libbacktrace-python.
 * This replaces the autoconf-generated config.h.
 */

#ifndef LIBBACKTRACE_CONFIG_H
#define LIBBACKTRACE_CONFIG_H

/* Enable GNU extensions for MAP_ANONYMOUS, dl_iterate_phdr, etc. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

/* Package information */
#define PACKAGE "libbacktrace"
#define PACKAGE_NAME "libbacktrace"
#define PACKAGE_VERSION "1.0"
#define PACKAGE_STRING "libbacktrace 1.0"

/* Sync and atomic functions */
#define HAVE_SYNC_FUNCTIONS 1
#define HAVE_ATOMIC_FUNCTIONS 1

/* Standard headers */
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_MMAN_H 1
#define STDC_HEADERS 1

/* DWARF support */
#define HAVE_DWARF 1

/* Compression support - disabled for simplicity */
/* #undef HAVE_LIBLZMA */
/* #undef HAVE_ZLIB */
/* #undef HAVE_ZSTD */

/* Platform-specific configuration */
#if defined(__APPLE__)
    /* macOS uses Mach-O format */
    #define BACKTRACE_SUPPORTS_DATA 1
    #define BACKTRACE_USES_MALLOC 1
    #define HAVE_MACH_O_DYLD_H 1
    /* macOS doesn't have dl_iterate_phdr */
    
#elif defined(__linux__)
    /* Linux uses ELF format */
    #define BACKTRACE_SUPPORTS_DATA 1
    #define BACKTRACE_USES_MALLOC 0
    #define HAVE_MMAP 1
    #define HAVE_DL_ITERATE_PHDR 1
    #define HAVE_LINK_H 1
    #define HAVE_GETIPINFO 1
    #define HAVE_DECL_STRNLEN 1
    
    /* ELF size based on architecture */
    #if defined(__LP64__) || defined(_LP64) || defined(__x86_64__) || defined(__aarch64__)
        #define BACKTRACE_ELF_SIZE 64
    #else
        #define BACKTRACE_ELF_SIZE 32
    #endif
    
#endif

/* Threading */
#define HAVE_PTHREAD 1

/* Large file support */
#define _LARGE_FILES 1
#define _FILE_OFFSET_BITS 64

#endif /* LIBBACKTRACE_CONFIG_H */
