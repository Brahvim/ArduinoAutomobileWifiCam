#pragma once

#define ifu(x) if (__builtin_expect(x, 0))
#define ifl(x) if (__builtin_expect(x, 1))
