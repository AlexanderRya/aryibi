#ifndef ARYIBI_ARYIBI_ASSERT_HPP
#define ARYIBI_ARYIBI_ASSERT_HPP

#include <cassert>
#ifdef __ASSERT_FUNCTION
#    define ARYIBI_ASSERT(x, msg)                                                                  \
        (!!(x)) ? (void)(0) : __assert_fail("[aryibi] " msg, __FILE__, __LINE__, __ASSERT_FUNCTION)
#    define ARYIBI_LOG(x) printf("[aryibi] %s", x)
#else
#    define ARYIBI_ASSERT(x, msg)
#    define ARYIBI_LOG(x)
#endif

#endif // ARYIBI_ARYIBI_ASSERT_HPP
