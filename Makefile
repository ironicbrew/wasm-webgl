# WebAssembly Build Configuration
# Requires: Emscripten SDK (emcc)

CC = emcc
OUT_DIR = build

# Compiler flags explained:
#   -O2                    Optimization level (0-3, s for size)
#   -s WASM=1              Output WebAssembly (not asm.js)
#   -s EXPORTED_RUNTIME_METHODS  JS helper methods to include
#   -s ALLOW_MEMORY_GROWTH Allow dynamic memory allocation
#   --no-entry             No main() function required

CFLAGS = -O2 \
         -s WASM=1 \
         -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPF32","HEAPU8","HEAP32"]' \
         -s EXPORTED_FUNCTIONS='["_malloc","_free"]' \
         -s ALLOW_MEMORY_GROWTH=1 \
         --no-entry

.PHONY: all clean serve basics memory canvas webgl

all: basics memory canvas webgl

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# Lesson 1: Basics
basics: $(OUT_DIR)
	$(CC) $(CFLAGS) src/main.c -o $(OUT_DIR)/main.js
	@echo "Built: main.js + main.wasm"

# Lesson 2: Memory
memory: $(OUT_DIR)
	$(CC) $(CFLAGS) src/memory.c -o $(OUT_DIR)/memory.js
	@echo "Built: memory.js + memory.wasm"

# Lesson 3: Canvas rendering
canvas: $(OUT_DIR)
	$(CC) $(CFLAGS) src/canvas.c -o $(OUT_DIR)/canvas.js -lm
	@echo "Built: canvas.js + canvas.wasm"

# Lesson 4: WebGL
webgl: $(OUT_DIR)
	$(CC) $(CFLAGS) -s USE_WEBGL2=1 src/webgl.c -o $(OUT_DIR)/webgl.js -lm
	@echo "Built: webgl.js + webgl.wasm"

clean:
	rm -rf $(OUT_DIR)

# Simple local server (Python 3)
serve:
	@echo "Starting server at http://localhost:8080"
	python3 -m http.server 8080
