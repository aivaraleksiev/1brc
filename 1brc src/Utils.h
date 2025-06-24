#pragma once

#if defined(_MSC_VER)
   #define ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
   #define ALWAYS_INLINE __attribute__((always_inline)) inline
#else
   #define ALWAYS_INLINE inline // fallback
#endif
