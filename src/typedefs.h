#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#define nullptr    	0

//  CountOf macro computes the number of elements in a statically-allocated array.
#if _MSC_VER >= 1400
    #define CountOf(x) _countof(x)
#else
    #define CountOf(x) (sizeof(x)/sizeof(x[0]))
#endif

#define ARRAYELEMCOUNT(x)    CountOf(x)

//Compile time assert. 
#define STATIC_ASSERT(expr)    		C_ASSERT(expr)


typedef float float32;


#if !_HAS_TR1
    namespace std
    { 
    	namespace tr1
    	{
    		template <class T> struct has_trivial_assign {static const bool value = false;};

    		#define SPECIALIZE_TRIVIAL_ASSIGN(type) template <> struct has_trivial_assign<type> {static const bool value = true;}
    		SPECIALIZE_TRIVIAL_ASSIGN(int8_t);
    		SPECIALIZE_TRIVIAL_ASSIGN(uint8_t);
    		SPECIALIZE_TRIVIAL_ASSIGN(int16_t);
    		SPECIALIZE_TRIVIAL_ASSIGN(uint16_t);
    		SPECIALIZE_TRIVIAL_ASSIGN(int32_t);
    		SPECIALIZE_TRIVIAL_ASSIGN(uint32_t);
    		SPECIALIZE_TRIVIAL_ASSIGN(int64_t);
    		SPECIALIZE_TRIVIAL_ASSIGN(uint64_t);
    		#undef SPECIALIZE_TRIVIAL_ASSIGN
    	};
    };
#endif


#endif
