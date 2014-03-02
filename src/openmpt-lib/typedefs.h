#ifndef TYPEDEFS_H
#define TYPEDEFS_H

//  CountOf macro computes the number of elements in a statically-allocated array.
#if _MSC_VER >= 1400
    #define CountOf(x) _countof(x)
#else
    #define CountOf(x) (sizeof(x)/sizeof(x[0]))
#endif

#endif