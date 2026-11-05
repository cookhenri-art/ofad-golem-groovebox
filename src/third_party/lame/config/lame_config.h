/* Groovebox — configuration portable de LAME 3.100 (encodeur MP3, LGPL), sans assembleur ni SSE :
   Windows (MSVC), Linux (GCC/Clang), Android (NDK). Remplace le config.h produit par « configure ». */
#ifndef LAME_CONFIG_H
#define LAME_CONFIG_H
#define STDC_HEADERS 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_STRCHR 1
#define HAVE_MEMCPY 1
#define LAME_LIBRARY_BUILD 1
#define PACKAGE "lame"
#define PACKAGE_VERSION "3.100"
#define VERSION "3.100"
#define USE_FAST_LOG 1
#include <stdint.h>
#include <inttypes.h>
typedef float ieee754_float32_t;
typedef double ieee754_float64_t;
typedef long double ieee854_float80_t;
#ifdef _MSC_VER
#pragma warning(disable: 4018 4100 4127 4244 4267 4305 4310 4456 4457 4701 4706 4996)
#endif
#endif
