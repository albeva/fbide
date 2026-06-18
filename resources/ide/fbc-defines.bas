'' Built-in #define probe for FBIde intellisense.
''
'' FBIde compiles this with `fbc -c` at startup and captures the `FBDEF <name>`
'' lines emitted by #print — those name the compiler's predefined symbols for the
'' active target. The intellisense preprocessor evaluator uses them (plus -d
'' command-line defines) to decide which `#if`/`#ifdef` branches are live.
''
'' Only *presence* macros are probed: operating-system and architecture symbols,
'' each defined solely on its target, so an absent one means "not this target".
'' Value macros (__FB_VERSION__, __FB_OUT_*__, __FB_OPTION_*__, …) are always
'' defined and would mislead a value check, so they are intentionally omitted.
''
'' Keep this list in sync with kKnownBuiltins in PpConditional.cpp. This file is
'' never compiled into a user program.

'' --- operating system / platform ---
#ifdef __FB_UNIX__
    #print FBDEF __FB_UNIX__
#endif
#ifdef __FB_LINUX__
    #print FBDEF __FB_LINUX__
#endif
#ifdef __FB_WIN32__
    #print FBDEF __FB_WIN32__
#endif
#ifdef __FB_DOS__
    #print FBDEF __FB_DOS__
#endif
#ifdef __FB_DARWIN__
    #print FBDEF __FB_DARWIN__
#endif
#ifdef __FB_FREEBSD__
    #print FBDEF __FB_FREEBSD__
#endif
#ifdef __FB_NETBSD__
    #print FBDEF __FB_NETBSD__
#endif
#ifdef __FB_OPENBSD__
    #print FBDEF __FB_OPENBSD__
#endif
#ifdef __FB_CYGWIN__
    #print FBDEF __FB_CYGWIN__
#endif
#ifdef __FB_JS__
    #print FBDEF __FB_JS__
#endif
#ifdef __FB_XBOX__
    #print FBDEF __FB_XBOX__
#endif
#ifdef __FB_ANDROID__
    #print FBDEF __FB_ANDROID__
#endif

'' --- architecture ---
#ifdef __FB_64BIT__
    #print FBDEF __FB_64BIT__
#endif
#ifdef __FB_ARM__
    #print FBDEF __FB_ARM__
#endif
#ifdef __FB_BIGENDIAN__
    #print FBDEF __FB_BIGENDIAN__
#endif
