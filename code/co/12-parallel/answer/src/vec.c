#include "vec.h"

#include <arm_neon.h>

void vec_scalar(const float *a, float *c, int n, int rounds) {
    int i;
#pragma clang loop vectorize(disable) interleave(disable) unroll(disable)
    for (i = 0; i < n; i++) {
        float x = a[i];
        int k;
        for (k = 0; k < rounds; k++) {
            x = x * 1.000001f + 1.0f;
        }
        c[i] = x;
    }
}

void vec_neon(const float *a, float *c, int n, int rounds) {
    float32x4_t scale = vdupq_n_f32(1.000001f);
    float32x4_t bias = vdupq_n_f32(1.0f);
    int i;
    for (i = 0; i < n; i += 4) {
        float32x4_t x = vld1q_f32(a + i);
        int k;
        for (k = 0; k < rounds; k++) {
            x = vmlaq_f32(bias, x, scale);
        }
        vst1q_f32(c + i, x);
    }
}
