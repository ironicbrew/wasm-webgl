@echo off
setlocal

set CFLAGS=-O2 -s WASM=1 -s EXPORTED_RUNTIME_METHODS=["ccall","cwrap","HEAPF32","HEAPU8","HEAP32"] -s EXPORTED_FUNCTIONS=["_malloc","_free","_init_webgl","_init_grid","_render_grid","_set_cell_color","_set_cell_text","_update_grid_buffer","_get_cell_at","_set_cursor"] -s ALLOW_MEMORY_GROWTH=1 --no-entry

if not exist src\wasm mkdir src\wasm

echo Building webgl...
call "C:\Users\rob.mclean\Desktop\Code\emcc\emsdk\upstream\emscripten\emcc.bat" %CFLAGS% -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORT_NAME=createWebGLModule -s USE_WEBGL2=1 c/webgl.c -o src/wasm/webgl.js -lm

echo Done.