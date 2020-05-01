#ifndef HSLCLIENT_EXPORT_H
#define HSLCLIENT_EXPORT_H

/** \def HSL_CALL
 * \ingroup HSL_client_misc
 * HSLService's Windows calling convention.
 *
 * Under Windows, the selection of available compilers and configurations
 * means that, unlike other platforms, there is not <em>one true calling
 * convention</em> (calling convention: the manner in which parameters are
 * passed to functions in the generated assembly code).
 *
 * Matching the Windows API itself, HSLService's client API uses the 
 * __cdecl convention and guarantees that the library is compiled in this way. 
 * The public header file also includes appropriate annotations so that 
 * your own software will use the right convention, even if another convention 
 * is being used by default within your codebase.
 *
 * On non-Windows operating systems, this macro is defined as nothing. This
 * means that you can apply it to your code without worrying about
 * cross-platform compatibility.
 */
#ifndef HSL_CALL
    #if defined _WIN32 || defined __CYGWIN__
        #define HSL_CALL __cdecl
    #else
        #define HSL_CALL
    #endif
#endif

#ifndef HSL_EXTERN_C
    #ifdef __cplusplus
        #define HSL_EXTERN_C extern "C"
    #else
        #define HSL_EXTERN_C
    #endif
#endif

#ifndef HSL_PUBLIC_FUNCTION
    #if defined(HSLService_EXPORTS)  // CMake-defined when creating shared library
        #if defined _WIN32 || defined __CYGWIN__
            #define HSL_PUBLIC_FUNCTION(rval)       HSL_EXTERN_C    __declspec(dllexport)                   rval    HSL_CALL
            #define HSL_PUBLIC_CLASS                                __declspec(dllexport)
            #define HSL_PRIVATE_FUNCTION(rval)                                                              rval    HSL_CALL
            #define HSL_PRIVATE_CLASS
        #else  // Not Windows
            #if __GNUC__ >= 4
                #define HSL_PUBLIC_FUNCTION(rval)   HSL_EXTERN_C    __attribute__((visibility("default")))  rval    HSL_CALL
                #define HSL_PUBLIC_CLASS                            __attribute__((visibility("default")))
            #else
                #define HSL_PUBLIC_FUNCTION(rval)   HSL_EXTERN_C rval HSL_CALL
                #define HSL_PUBLIC_CLASS
            #endif
            #define HSL_PRIVATE_FUNCTION(rval)                      __attribute__((visibility("hidden")))   rval    HSL_CALL
            #define HSL_PRIVATE_CLASS                               __attribute__((visibility("hidden")))
        #endif  //defined _WIN32 || defined __CYGWIN__
    #elif defined(HSLService_STATIC) // CMake-defined when creating static library
        #define HSL_PUBLIC_FUNCTION(rval)           HSL_EXTERN_C                                            rval    HSL_CALL
        #define HSL_PUBLIC_CLASS
        #define HSL_PRIVATE_FUNCTION(rval)                                                                  rval    HSL_CALL
        #define HSL_PRIVATE_CLASS
    #else //This DLL/so/dylib is being imported
        #if defined _WIN32 || defined __CYGWIN__
            #define HSL_PUBLIC_FUNCTION(rval)       HSL_EXTERN_C    __declspec(dllimport)                   rval    HSL_CALL
            #define HSL_PUBLIC_CLASS                                __declspec(dllimport)
        #else  // Not Windows
            #define HSL_PUBLIC_FUNCTION(rval)       HSL_EXTERN_C                                            rval    HSL_CALL
            #define HSL_PUBLIC_CLASS
        #endif  //defined _WIN32 || defined __CYGWIN__
        #define HSL_PRIVATE_FUNCTION(rval)                                                                  rval    HSL_CALL
        #define HSL_PRIVATE_CLASS
    #endif //HSLClient_EXPORTS
#endif //!defined(HSL_PUBLIC_FUNCTION)

/*
 For any CPP API we decide to expose:
 We want those classes/functions to NOT be exported/imported when building/using
 the C API, but we do want them exported/imported when building/using the C++ API.
 */

#if !defined(HSL_CPP_PUBLIC_FUNCTION) && defined(HSLSERVICE_CPP_API)
    #define HSL_CPP_PUBLIC_FUNCTION(rval) HSL_PUBLIC_FUNCTION(rval)
    #define HSL_CPP_PUBLIC_CLASS HSL_PUBLIC_CLASS
    #define HSL_CPP_PRIVATE_FUNCTION(rval) HSL_PRIVATE_FUNCTION(rval)
    #define HSL_CPP_PRIVATE_CLASS HSL_PRIVATE_CLASS
#else
    #define HSL_CPP_PUBLIC_FUNCTION(rval) HSL_PRIVATE_FUNCTION(rval)
    #define HSL_CPP_PUBLIC_CLASS HSL_PRIVATE_CLASS
    #define HSL_CPP_PRIVATE_FUNCTION(rval) HSL_PRIVATE_FUNCTION(rval)
    #define HSL_CPP_PRIVATE_CLASS HSL_PRIVATE_CLASS
#endif

#endif // HSLCLIENT_EXPORT_H