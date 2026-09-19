// oscpm: OSC address pattern matching
//
// Copyright (c) 2026 Jamie Bullock
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

// The library version, for a consumer that pins or branches on it. The
// three components are integers, usable in #if; OSCPM_VERSION is the same
// value as a string, for a log line. The CMake project version must agree
// with these, and the build checks that it does.
//
// oscpm follows semantic versioning. While the major version is 0 the
// public API may change in a minor release; from 1.0 it changes only in a
// major release. Every release is a git tag named after the version.

#pragma once

#define OSCPM_VERSION_MAJOR 0
#define OSCPM_VERSION_MINOR 1
#define OSCPM_VERSION_PATCH 0
#define OSCPM_VERSION "0.1.0"
