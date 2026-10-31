#pragma once

#if defined(_WIN32)
    #if defined(RX_BUILD_DLL)
        #define RX_API __declspec(dllexport)
    #elif defined(RX_USE_DLL)
        #define RX_API __declspec(dllimport)
    #else
        #define RX_API
    #endif
#else
    #define RX_API
#endif
