#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;
typedef float f32;
typedef double f64;
typedef int b32;

#ifdef __cplusplus
}
#endif

#ifndef __cplusplus
#ifndef __bool_true_false_are_defined
#define bool u32
#define true 1
#define false 0
#endif
#endif
