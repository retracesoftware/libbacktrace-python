/* backtrace-supported.h -- Whether stack backtrace is supported.
   Copyright (C) 2012-2024 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Google.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    (1) Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    (2) Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

    (3) The name of the author may not be used to
    endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.  */

/* Generated for libbacktrace-python */

#ifndef BACKTRACE_SUPPORTED_H
#define BACKTRACE_SUPPORTED_H

#if defined(__linux__) || defined(__APPLE__)
    /* Backtrace is supported on Linux and macOS */
    #define BACKTRACE_SUPPORTED 1
    #define BACKTRACE_SUPPORTS_THREADS 1
    #define BACKTRACE_SUPPORTS_DATA 1
    
    #if defined(__APPLE__)
        /* macOS uses malloc for allocations */
        #define BACKTRACE_USES_MALLOC 1
    #else
        /* Linux can use mmap */
        #define BACKTRACE_USES_MALLOC 0
    #endif
    
#else
    /* Unsupported platform */
    #define BACKTRACE_SUPPORTED 0
    #define BACKTRACE_USES_MALLOC 1
    #define BACKTRACE_SUPPORTS_THREADS 0
    #define BACKTRACE_SUPPORTS_DATA 0
    
#endif

#endif /* BACKTRACE_SUPPORTED_H */
