@echo off
setlocal

set CFLAGS=-O2 -s WASM=1 -s EXPORTED_RUNTIME_METHODS=["ccall","cwrap","HEAPF32","HEAPU8","HEAP32"] -s EXPORTED_FUNCTIONS=["_malloc","_free"] -s ALLOW_MEMORY_GROWTH=1 --no-entry

if not exist public\build mkdir public\build

echo Building webgl...
call "C:\Users\rob.mclean\Desktop\Code\emcc\emsdk\upstream\emscripten\emcc.bat" %CFLAGS% -s USE_WEBGL2=1 c/webgl.c -o public/build/webgl.js -lm

echo Done.