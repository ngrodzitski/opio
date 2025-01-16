// clang-format off
#pragma once

#define OPIO_VERSION_CODE( major, minor, patch ) \
    ( ( ( major ) << 16UL ) + ( ( minor ) << 8UL ) + ( ( patch ) << 0UL ))

#define OPIO_VERSION_MAJOR 0ull
#define OPIO_VERSION_MINOR 1ull
#define OPIO_VERSION_PATCH 0ull

#if !defined(OPIO_VCS_REVISION)
    #define OPIO_VCS_REVISION "n/a"
#endif

#define OPIO_VERSION \
    OPIO_VERSION_CODE( OPIO_VERSION_MAJOR, OPIO_VERSION_MINOR, OPIO_VERSION_PATCH )

