# WebAssembly + C Learning Project

A step-by-step project for learning C → WebAssembly → WebGL, ultimately for high-performance table rendering.

## Current Stage: Basics

Learning objectives:
- [x] Project structure
- [ ] Install Emscripten
- [ ] Compile C to WebAssembly
- [ ] Call C functions from JavaScript
- [ ] Understand memory management

## Setup

### 1. Install Emscripten SDK

```bash
# Clone the SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Download and install the latest SDK
./emsdk install latest
./emsdk activate latest

# Set up environment (add to your shell profile for persistence)
source ./emsdk_env.sh
```

Verify installation:
```bash
emcc --version
```

### 2. Build the Project

```bash
make
```

This compiles `src/main.c` → `build/main.wasm` + `build/main.js`

### 3. Run Locally

WebAssembly requires a web server (can't load from file://):

```bash
make serve
# Open http://localhost:8080
```

## Project Structure

```
webgl/
├── src/
│   └── main.c          # C source code
├── build/              # Generated WebAssembly output
│   ├── main.wasm       # Binary WebAssembly module
│   └── main.js         # JavaScript glue code
├── index.html          # Web interface
├── Makefile            # Build configuration
└── README.md
```

## Learning Path

1. **Basics** (current) - Simple function exports, calling C from JS
2. **Memory** - Passing arrays/buffers between C and JS
3. **Canvas 2D** - Drawing with C, understanding the render loop
4. **WebGL Intro** - Basic shaders, vertex buffers
5. **WebGL Tables** - Rendering data grids efficiently
6. **React Integration** - Using WebGL canvas in React components

## Key Concepts

### EMSCRIPTEN_KEEPALIVE
Prevents the compiler from removing "unused" functions during optimization.
Functions marked with this are accessible from JavaScript.

### Module._functionName()
How you call exported C functions from JS. The underscore prefix is an Emscripten convention.

### ccall / cwrap
Helper functions for calling C code with type conversion:
```javascript
// Direct call with type conversion
Module.ccall('add', 'number', ['number', 'number'], [5, 3]);

// Create a reusable JS function
const add = Module.cwrap('add', 'number', ['number', 'number']);
add(5, 3);
```
