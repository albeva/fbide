//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// macOS move-to-trash, backed by NSFileManager. Compiled only on Apple
// platforms (see src/lib/CMakeLists.txt). Deliberately does not use the
// project's wxWidgets precompiled header.
//
#ifdef __APPLE__

#import <Foundation/Foundation.h>

#include "utils/PlatformTrash.hpp"

auto fbide::moveToTrash(const std::filesystem::path& path) -> bool {
    @autoreleasepool {
        NSString* const nsPath = [NSString stringWithUTF8String:path.c_str()];
        if (nsPath == nil) {
            return false;
        }
        NSURL* const url = [NSURL fileURLWithPath:nsPath];
        NSError* error = nil;
        const BOOL ok = [[NSFileManager defaultManager] trashItemAtURL:url
                                                      resultingItemURL:nil
                                                                 error:&error];
        return ok == YES;
    }
}

#endif // __APPLE__
