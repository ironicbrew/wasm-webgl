// Step 1: Basic C functions exported to WebAssembly
// These will be callable from JavaScript

#include <emscripten.h>

// EMSCRIPTEN_KEEPALIVE prevents the compiler from removing "unused" functions
// This is essential for functions we want to call from JS

EMSCRIPTEN_KEEPALIVE
int add(int a, int b) {
    return a + b;
}

EMSCRIPTEN_KEEPALIVE
int multiply(int a, int b) {
    return a * b;
}

// Simple function to demonstrate memory/state
static int counter = 0;

EMSCRIPTEN_KEEPALIVE
int increment() {
    return ++counter;
}

EMSCRIPTEN_KEEPALIVE
int get_counter() {
    return counter;
}

EMSCRIPTEN_KEEPALIVE
void reset_counter() {
    counter = 0;
}
