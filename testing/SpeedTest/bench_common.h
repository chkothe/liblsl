#ifndef BOILERPLATE_H_INCLUDED
#define BOILERPLATE_H_INCLUDED

// boilerplate so we can link against raw library bits available in CMake scope instead of fully-built lib
extern "C" { const char *lsl_library_info() { return ""; } }

#endif