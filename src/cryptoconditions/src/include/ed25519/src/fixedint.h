/*
    Portable header to provide the 32 and 64 bits type.

    Not a compatible replacement for <stdint.h>, do not blindly use it as such.
*/

#if ((defined(__STDC__) && __STDC__ && __STDC_VERSION__ >= 199901L) || (defined(__WATCOMC__) && (defined(_STDINT_H_INCLUDED) || __WATCOMC__ >= 1250)) || (defined(__GNUC__) && (defined(_STDINT_H) || defined(_STDINT_H_) || defined(__UINT_FAST64_TYPE__)) )) && !defined(FIXEDINT_H_INCLUDED)
    #include <stdint.h>
    #define FIXEDINT_H_INCLUDED

    #if defined(__WATCOMC__) && __WATCOMC__ >= 1250 && !defined(UINT64_C)
        #include <limits.h>
        #define UINT64_C(x) (x + (UINT64_MAX - UINT64_MAX))
    #endif
#endif


#ifndef FIXEDINT_H_INCLUDED
    #define FIXEDINT_H_INCLUDED
    
    #include <limits.h>

    /* (u)int32_t */
    #ifndef uint32_t
        #if (ULONG_MAX == 0xffffffffUL)
            typedef unsigned long uint32_t;
        #elif (UINT_MAX == 0xffffffffUL)
            typedef uns