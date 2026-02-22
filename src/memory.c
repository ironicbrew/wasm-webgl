
#include <emscripten.h>
#include <stdlib.h>
#include <math.h>

EMSCRIPTEN_KEEPALIVE
float sum_floats(float* data, int length) {
    float sum = 0.0f;
    for (int i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

EMSCRIPTEN_KEEPALIVE
void find_min_max(float* data, int length, float* out_min, float* out_max) {
    if (length == 0) return;
    
    // KEY FIX: Initialize min/max to the FIRST element, not 0!
    // If you init to 0 and all values are positive, min stays 0 (wrong)
    // If you init to 0 and all values are negative, max stays 0 (wrong)
    *out_min = data[0];
    *out_max = data[0];
    
    // Start loop at 1 since we already used data[0]
    for (int i = 1; i < length; i++) {
        if (data[i] < *out_min) {
            *out_min = data[i];
        }
        if (data[i] > *out_max) {
            *out_max = data[i];
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void normalize_in_place(float* data, int length) {
    if (length == 0) return;
    
    float min, max;
    find_min_max(data, length, &min, &max);
    
    // Avoid division by zero if all values are the same
    float range = max - min;
    if (range == 0.0f) range = 1.0f;
    
    for (int i = 0; i < length; i++) {
        data[i] = (data[i] - min) / range;
    }
}

EMSCRIPTEN_KEEPALIVE
float* generate_sequence(int length, float start, float step) {
    float* result = (float*)malloc(length * sizeof(float));
    if (!result) return NULL;
    
    for (int i = 0; i < length; i++) {
        result[i] = start + (i * step);
    }
    
    return result;
}

EMSCRIPTEN_KEEPALIVE
int get_float_size() {
    return sizeof(float);
}
