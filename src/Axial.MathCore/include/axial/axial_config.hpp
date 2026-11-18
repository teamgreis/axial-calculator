// Axial Math Core — configuration & export macros
#pragma once

#define AXIAL_VERSION_MAJOR 0
#define AXIAL_VERSION_MINOR 1
#define AXIAL_VERSION_PATCH 0
#define AXIAL_VERSION_STRING "0.1.0"

// DLL export macro (Windows). On other platforms the macro is empty.
#if defined(_WIN32) || defined(_WIN64)
  #ifdef AXIAL_BUILD_DLL
    #define AXIAL_API __declspec(dllexport)
  #else
    #define AXIAL_API __declspec(dllimport)
  #endif
#else
  #define AXIAL_API
#endif