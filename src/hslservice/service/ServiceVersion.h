#ifndef SERVICE_VERSION_H
#define SERVICE_VERSION_H

/// Conventional string-ification macro.
// From: http://stackoverflow.com/questions/5256313/c-c-macro-string-concatenation
#if !defined(HSL_STRINGIZE)
    #define HSL_STRINGIZEIMPL(x) #x
    #define HSL_STRINGIZE(x)     HSL_STRINGIZEIMPL(x)
#endif

// Current version of this SERVICE
#define HSL_SERVICE_VERSION_PRODUCT 0
#define HSL_SERVICE_VERSION_MAJOR   1
#define HSL_SERVICE_VERSION_MINOR   0
#define HSL_SERVICE_VERSION_HOTFIX  0

/// "Product.Major.Minor.Hotfix"
#if !defined(HSL_SERVICE_VERSION_STRING)
    #define HSL_SERVICE_VERSION_STRING HSL_STRINGIZE(HSL_SERVICE_VERSION_PRODUCT.HSL_SERVICE_VERSION_MAJOR.HSL_SERVICE_VERSION_MINOR.HSL_SERVICE_VERSION_HOTFIX)
#endif

#endif // SERVICE_VERSION_H
