#pragma once

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// Orka Version
#define ORKA_VERSION_CODENAME "Spring Tide"
#define ORKA_VERSION_SERIES 2
#define ORKA_VERSION_NUMBER 0    // ORKA_VERSION_SERIES.ORKA_VERSION_NUMBER -> e.g. 2.1
#define ORKA_IS_ALPHA false  // Ultra early development stage (probably unusable)
#define ORKA_IS_BETA true   // Early development stage (probably buggy) (ignored if ORKA_IS_ALPHA == true)

// Leviathan version
#define LEVIATHAN_VERSION_SERIES 1
#define LEVIATHAN_VERSION_NUMBER 0
#define LEVIATHAN_VERSION_STRING "LEVIATHAN_" STR(LEVIATHAN_VERSION_SERIES) "." STR(LEVIATHAN_VERSION_NUMBER) "@ORKA_" STR(ORKA_VERSION_SERIES) "." STR(ORKA_VERSION_NUMBER) // Will produce something like "LEVIATHAN_1.0@ORKA_1.6"


// Well...
#define DROELF  0xD50E2F
#define OELF    0x0E2F