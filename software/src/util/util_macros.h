#ifndef UTIL_MACROS_H_
#define UTIL_MACROS_H_

#include <stdint.h>
#include <string.h>

#ifndef SWAP
#define SWAP(a, b) do { typeof(a) t = a; a = b; b = t; } while(0)
#endif

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete


#define CLIP(x) if (x < -32767) x = -32767; if (x > 32767) x = 32767;
#define CONSTRAIN(x, lb, ub) do { if (x < (lb)) x = lb; else if (x > (ub)) x = ub; } while (0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define MACRO_CONCAT_(x, y) x ## y
#define MACRO_CONCAT(x, y) MACRO_CONCAT_(x, y)

#endif // UTIL_MACROS_H_
