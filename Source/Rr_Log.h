#pragma once

#include <stdio.h>
#include <stdlib.h>

#define RR_LOG(...)                          \
    do                                       \
    {                                        \
        fprintf(stderr, "%s(): ", __func__); \
        fprintf(stderr, __VA_ARGS__);        \
        fprintf(stderr, "\n");               \
    }                                        \
    while(0)

#define RR_ABORT(...)                        \
    do                                       \
    {                                        \
        fprintf(stderr, "%s(): ", __func__); \
        fprintf(stderr, __VA_ARGS__);        \
        fprintf(stderr, "\n");               \
        abort();                             \
    }                                        \
    while(0)
