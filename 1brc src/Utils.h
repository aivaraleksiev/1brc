// Author: Ayvar Aleksiev 2025

#pragma once

#if defined(_MSC_VER)
   #define ALWAYS_INLINE __forceinline
   #define BEGIN_HOT_SEGMENT __pragma(code_seg(push, hot_seg, ".text$hot"))
   #define END_HOT_SEGMENT   __pragma(code_seg(pop, hot_seg))
#elif defined(__GNUC__) || defined(__clang__)
   #define ALWAYS_INLINE __attribute__((always_inline, hot)) inline
   #define BEGIN_HOT_SEGMENT
   #define END_HOT_SEGMENT
#else
   // fallback
   #define ALWAYS_INLINE inline
   #define BEGIN_HOT_SEGMENT
   #define END_HOT_SEGMENT
#endif
