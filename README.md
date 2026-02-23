# WebGL + WASM Interactive Grid

A React + Vite app that renders an interactive data grid using WebGL from C (compiled to WebAssembly).

## Setup

### 1. Install Emscripten SDK (for building the WASM)

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh   # or emsdk_env.bat on Windows
```

### 2. Install dependencies

```bash
pnpm install
```

### 3. Build the WebAssembly module

```bash
pnpm run build:wasm
```

This compiles `c/webgl.c` → `public/build/webgl.js` + `public/build/webgl.wasm`

### 4. Run the dev server

```bash
pnpm dev
```

Open http://localhost:5173

## Project Structure

```
wasm-webgl/
├── c/
│   └── webgl.c         # C source (WebGL grid rendering)
├── public/
│   └── build/          # WASM output (webgl.js, webgl.wasm)
├── src/
│   ├── components/
│   │   └── WebGLGrid.jsx
│   ├── App.jsx
│   └── main.jsx
├── build.bat           # Windows build script for WASM
├── package.json
└── vite.config.js
```
