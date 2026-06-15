//
// FBIde editor for FreeBASIC - https://freebasic.net
// Copyright (c) 2026 Albert Varaksin
// Licensed under the MIT License. See LICENSE file for details.
// https://github.com/albeva/fbide
//
// Native macOS file/folder icons for the file browser tree. wxGenericDirCtrl
// otherwise draws flat generic glyphs from wxArtProvider; here we pull the real
// Finder icons (which include fbide's own registered .bas/.bi/.fbs icons).
//
#include "MacFileIcons.hpp"
#include <wx/image.h>
#include <algorithm>
#include <cmath>
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace {

/// Render an NSImage into a square wxBitmap at `logicalPx`, drawing into a
/// `logicalPx * scale` backing store so it is crisp on Retina.
auto toBitmap(NSImage* icon, const int logicalPx, const double scale) -> wxBitmap {
    if (icon == nil) {
        return wxNullBitmap;
    }
    const int phys = std::max(1, static_cast<int>(std::lround(logicalPx * scale)));

    // Premultiplied RGBA: the only layout Core Graphics will actually draw into.
    // A non-premultiplied bitmap context silently produces a blank image.
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nullptr
                      pixelsWide:phys
                      pixelsHigh:phys
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:0
                    bitsPerPixel:0];
    if (rep == nil) {
        return wxNullBitmap;
    }

    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [icon drawInRect:NSMakeRect(0, 0, phys, phys)
            fromRect:NSZeroRect
           operation:NSCompositingOperationSourceOver
            fraction:1.0];
    [NSGraphicsContext restoreGraphicsState];

    wxImage image(phys, phys);
    image.SetAlpha();
    const unsigned char* src = [rep bitmapData];
    const NSInteger stride = [rep bytesPerRow];
    unsigned char* rgb = image.GetData();
    unsigned char* alpha = image.GetAlpha();
    for (int y = 0; y < phys; ++y) {
        const unsigned char* row = src + (y * stride);
        for (int x = 0; x < phys; ++x) {
            const unsigned char* px = row + (x * 4);
            const int dst = (y * phys) + x;
            const unsigned int a = px[3];
            // wxImage wants straight (non-premultiplied) RGB + a separate alpha
            // plane, so divide the colour back out.
            if (a == 0) {
                rgb[(dst * 3) + 0] = 0;
                rgb[(dst * 3) + 1] = 0;
                rgb[(dst * 3) + 2] = 0;
            } else {
                rgb[(dst * 3) + 0] = static_cast<unsigned char>(std::min(255U, (px[0] * 255U) / a));
                rgb[(dst * 3) + 1] = static_cast<unsigned char>(std::min(255U, (px[1] * 255U) / a));
                rgb[(dst * 3) + 2] = static_cast<unsigned char>(std::min(255U, (px[2] * 255U) / a));
            }
            alpha[dst] = static_cast<unsigned char>(a);
        }
    }
    return wxBitmap(image, wxBITMAP_SCREEN_DEPTH, scale);
}

/// Backing-store scale of the main display (2.0 on Retina). Read here rather
/// than from the wxWindow, whose scale factor is still 1.0 during the deferred
/// startup re-icon pass (the tree isn't on its final display yet).
auto backingScale() -> double {
    NSScreen* screen = [NSScreen mainScreen];
    return screen != nil ? static_cast<double>(screen.backingScaleFactor) : 1.0;
}

} // namespace

namespace fbide {

auto nativeFolderIcon(const int logicalPx) -> wxBitmap {
    return toBitmap([[NSWorkspace sharedWorkspace] iconForContentType:UTTypeFolder], logicalPx, backingScale());
}

auto nativeFileIcon(const wxString& ext, const int logicalPx) -> wxBitmap {
    UTType* type = nil;
    if (!ext.IsEmpty()) {
        type = [UTType typeWithFilenameExtension:[NSString stringWithUTF8String:ext.utf8_str()]];
    }
    if (type == nil) {
        type = UTTypeData; // generic document
    }
    return toBitmap([[NSWorkspace sharedWorkspace] iconForContentType:type], logicalPx, backingScale());
}

} // namespace fbide
