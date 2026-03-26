#ifndef BENCH_LENET_H
#define BENCH_LENET_H

#include "bench_common.h"
#include "lenet_data.h"

static inline void requantivate_u8_relu(const int32_t *acc, uint8_t *out, int length, float in_scale, float w_scale, float out_scale) {
    float real_scale = (in_scale * w_scale) / out_scale;
    for (int i = 0; i < length; i++) {
        float fval = (float)acc[i] * real_scale;
        int32_t val = (int32_t)fval; // round towards zero
        if (val < 0) val = 0; // relu
        if (val > 127) val = 127;
        out[i] = (uint8_t)val;
    }
}

#endif
