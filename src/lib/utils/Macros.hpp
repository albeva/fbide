//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
#pragma once

#ifdef _MSC_VER
    #define FBIDE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FBIDE_INLINE __attribute__((always_inline)) inline
#else
    #define FBIDE_INLINE inline
#endif
