#ifndef __IVI_SDK_H__
#define __IVI_SDK_H__

#define IVI_SDK_API_VERSION 1   // should match IVISDKAPIVersion() call below

#if !defined(IVI_SDK_NOEXPORT)
    #if defined(_MSC_VER)
        #if defined(IVI_SDK_EXPORT)
            #define IVI_SDK_API __declspec(dllexport)
        #else
            #define IVI_SDK_API __declspec(dllimport)
        #endif // IVI_SDK_EXPORT
    #elif defined(__GNUC__)
        #if defined(IVI_SDK_EXPORT)
            #define IVI_SDK_API __attribute__((__visibility__("default")))
        #endif // IVI_SDK_EXPORT
    #endif // _MSC_VER / __GNUC__
#endif // !IVI_SDK_NOEXPORT

#ifndef IVI_SDK_API
#define IVI_SDK_API
#endif

extern "C" {

    IVI_SDK_API int         IVISDKAPIVersion();

} // extern "C"

#endif //__IVI_SDK_H__
