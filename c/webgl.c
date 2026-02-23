// WebGL Interactive Grid - GPU-Accelerated Rendering from C
//
// Grid cell backgrounds + text are BOTH rendered via WebGL shaders.
// Text uses a 5x7 bitmap font baked into a texture atlas.

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_ctx = 0;

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        printf("Shader compile error: %s\n", log);
        return 0;
    }
    return shader;
}

static GLuint create_program(const char* vert_src, const char* frag_src) {
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vert || !frag) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        printf("Program link error: %s\n", log);
        return 0;
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

static void ensure_context(void) {
    if (webgl_ctx > 0) {
        emscripten_webgl_make_context_current(webgl_ctx);
    }
}

// ============================================================
// INIT
// ============================================================

EMSCRIPTEN_KEEPALIVE
int init_webgl(int width, int height) {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = 1;
    attrs.depth = 0;
    attrs.antialias = 1;
    attrs.majorVersion = 1;

    webgl_ctx = emscripten_webgl_create_context("#grid-canvas", &attrs);
    if (webgl_ctx <= 0) {
        printf("Failed to create WebGL context\n");
        return 0;
    }
    emscripten_webgl_make_context_current(webgl_ctx);
    glViewport(0, 0, width, height);
    return 1;
}

// ============================================================
// GRID BACKGROUNDS
// ============================================================

static const char* grid_vertex_src =
    "attribute vec2 a_position;\n"
    "attribute vec3 a_color;\n"
    "varying vec3 v_color;\n"
    "void main() {\n"
    "    v_color = a_color;\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char* grid_fragment_src =
    "precision mediump float;\n"
    "varying vec3 v_color;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(v_color, 1.0);\n"
    "}\n";

static GLuint grid_program = 0;
static GLuint grid_vbo = 0;
static float* grid_vertices = NULL;
static int grid_vertex_count = 0;
static int grid_rows = 0;
static int grid_cols = 0;
static float* text_batch = NULL;

static void cell_to_clip(int row, int col, int total_rows, int total_cols,
                         float* x1, float* y1, float* x2, float* y2) {
    float cell_w = 2.0f / total_cols;
    float cell_h = 2.0f / total_rows;
    *x1 = -1.0f + col * cell_w + 0.005f;
    *x2 = -1.0f + (col + 1) * cell_w - 0.005f;
    *y1 = 1.0f - (row + 1) * cell_h + 0.005f;
    *y2 = 1.0f - row * cell_h - 0.005f;
}

EMSCRIPTEN_KEEPALIVE
int init_grid(int rows, int cols) {
    ensure_context();
    grid_rows = rows;
    grid_cols = cols;

    if (!grid_program) {
        grid_program = create_program(grid_vertex_src, grid_fragment_src);
        if (!grid_program) return 0;
    }

    int cells = rows * cols;
    grid_vertex_count = cells * 6;
    int floats_needed = grid_vertex_count * 5;

    if (grid_vertices) free(grid_vertices);
    grid_vertices = (float*)malloc(floats_needed * sizeof(float));

    if (text_batch) { free(text_batch); text_batch = NULL; }

    int idx = 0;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            float x1, y1, x2, y2;
            cell_to_clip(row, col, rows, cols, &x1, &y1, &x2, &y2);

            float r, g, b;
            if (row == 0) { r = 0.0f; g = 0.5f; b = 0.7f; }
            else if (row % 2 == 0) { r = 0.15f; g = 0.15f; b = 0.25f; }
            else { r = 0.2f; g = 0.2f; b = 0.32f; }

            float vdata[6][5] = {
                {x1,y1,r,g,b}, {x2,y1,r,g,b}, {x2,y2,r,g,b},
                {x1,y1,r,g,b}, {x2,y2,r,g,b}, {x1,y2,r,g,b}
            };
            for (int v = 0; v < 6; v++)
                for (int f = 0; f < 5; f++)
                    grid_vertices[idx++] = vdata[v][f];
        }
    }

    if (!grid_vbo) glGenBuffers(1, &grid_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, floats_needed * sizeof(float), grid_vertices, GL_DYNAMIC_DRAW);
    return 1;
}

EMSCRIPTEN_KEEPALIVE
void set_cell_color(int row, int col, int total_cols, float r, float g, float b) {
    if (!grid_vertices) return;
    ensure_context();
    int cell_idx = row * total_cols + col;
    int base = cell_idx * 6 * 5;
    for (int v = 0; v < 6; v++) {
        int offset = base + v * 5 + 2;
        grid_vertices[offset + 0] = r;
        grid_vertices[offset + 1] = g;
        grid_vertices[offset + 2] = b;
    }
}

EMSCRIPTEN_KEEPALIVE
void update_grid_buffer(void) {
    if (!grid_vertices || !grid_vbo) return;
    ensure_context();
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, grid_vertex_count * 5 * sizeof(float), grid_vertices);
}

static void render_grid_bg(void) {
    glUseProgram(grid_program);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    GLint a_pos = glGetAttribLocation(grid_program, "a_position");
    GLint a_col = glGetAttribLocation(grid_program, "a_color");
    glEnableVertexAttribArray(a_pos);
    glEnableVertexAttribArray(a_col);
    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glVertexAttribPointer(a_col, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, grid_vertex_count);
    glDisableVertexAttribArray(a_pos);
    glDisableVertexAttribArray(a_col);
}

// ============================================================
// TEXT RENDERING - Bitmap font texture atlas
// ============================================================
//
// How this works:
//   1. A 5x7 pixel bitmap for each character is embedded in C.
//   2. All bitmaps are packed into one small texture (the "atlas").
//   3. To draw a character, we draw a textured quad sampling
//      the right region of the atlas via UV coordinates.
//   4. The fragment shader uses the texture's red channel as alpha,
//      so the background shows through where the font pixel is 0.

// 5x7 bitmap font data. Each byte is a row, low 5 bits = pixels.
// Supports: 0-9 . + - C o l A B D E F G H I J K L M N O P Q R S T U V W X Y Z space
#define FONT_CHAR_COUNT 40

static const unsigned char font_5x7[FONT_CHAR_COUNT][7] = {
    { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E }, /*  0: '0' */
    { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E }, /*  1: '1' */
    { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F }, /*  2: '2' */
    { 0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E }, /*  3: '3' */
    { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 }, /*  4: '4' */
    { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E }, /*  5: '5' */
    { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E }, /*  6: '6' */
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }, /*  7: '7' */
    { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E }, /*  8: '8' */
    { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C }, /*  9: '9' */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C }, /* 10: '.' */
    { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 }, /* 11: '-' */
    { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 }, /* 12: '+' */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 13: ' ' (space) */
    { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, /* 14: 'A' */
    { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E }, /* 15: 'B' */
    { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E }, /* 16: 'C' */
    { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E }, /* 17: 'D' */
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F }, /* 18: 'E' */
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 }, /* 19: 'F' */
    { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F }, /* 20: 'G' */
    { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 }, /* 21: 'H' */
    { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E }, /* 22: 'I' */
    { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C }, /* 23: 'J' */
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 }, /* 24: 'K' */
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F }, /* 25: 'L' */
    { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 }, /* 26: 'M' */
    { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 }, /* 27: 'N' */
    { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, /* 28: 'O' */
    { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 }, /* 29: 'P' */
    { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D }, /* 30: 'Q' */
    { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 }, /* 31: 'R' */
    { 0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E }, /* 32: 'S' */
    { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, /* 33: 'T' */
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E }, /* 34: 'U' */
    { 0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04 }, /* 35: 'V' */
    { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 }, /* 36: 'W' */
    { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 }, /* 37: 'X' */
    { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 }, /* 38: 'Y' */
    { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F }, /* 39: 'Z' */
};

#define FONT_CHAR_W 5
#define FONT_CHAR_H 7
#define FONT_COLS 8
#define FONT_ATLAS_W (FONT_COLS * 6)
#define FONT_ATLAS_H (((FONT_CHAR_COUNT + FONT_COLS - 1) / FONT_COLS) * 8)
#define MAX_CELL_LEN 32
#define MAX_ROWS 256
#define MAX_COLS 64

static char cell_text[MAX_ROWS][MAX_COLS][MAX_CELL_LEN];

static int char_to_index(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c == '.') return 10;
    if (c == '-') return 11;
    if (c == '+') return 12;
    if (c == ' ') return 13;
    if (c >= 'A' && c <= 'Z') return 14 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 14 + (c - 'a');
    return -1;
}

// Text shader: textured quad per character
static const char* text_vertex_src =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

static const char* text_fragment_src =
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec3 u_color;\n"
    "void main() {\n"
    "    float a = texture2D(u_texture, v_uv).r;\n"
    "    gl_FragColor = vec4(u_color, a);\n"
    "}\n";

static GLuint text_program = 0;
static GLuint font_texture = 0;
static GLuint text_vbo = 0;

static void init_font_texture(void) {
    if (font_texture) return;

    unsigned char* pixels = (unsigned char*)calloc(FONT_ATLAS_W * FONT_ATLAS_H, 1);

    for (int c = 0; c < FONT_CHAR_COUNT; c++) {
        int atlas_col = c % FONT_COLS;
        int atlas_row = c / FONT_COLS;
        int ox = atlas_col * 6;
        int oy = atlas_row * 8;

        for (int y = 0; y < 7; y++) {
            unsigned char row_bits = font_5x7[c][y];
            for (int x = 0; x < 5; x++) {
                int px = ox + x;
                int py = oy + y;
                if (px < FONT_ATLAS_W && py < FONT_ATLAS_H) {
                    pixels[py * FONT_ATLAS_W + px] = (row_bits & (1 << (4 - x))) ? 255 : 0;
                }
            }
        }
    }

    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FONT_ATLAS_W, FONT_ATLAS_H, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(pixels);
}

// Batch all character quads into one draw call per frame
#define MAX_TEXT_VERTS (MAX_ROWS * MAX_COLS * MAX_CELL_LEN * 6)
// text_batch is forward-declared near the top so init_grid can free it on resize
static int text_batch_count = 0;

static void render_text(void) {
    if (!text_program) {
        text_program = create_program(text_vertex_src, text_fragment_src);
        if (!text_program) return;
    }
    init_font_texture();
    if (!font_texture) return;

    // Allocate batch buffer lazily
    if (!text_batch) {
        int max_chars = grid_rows * grid_cols * MAX_CELL_LEN;
        // 6 verts * 4 floats (x,y,u,v)
        text_batch = (float*)malloc(max_chars * 6 * 4 * sizeof(float));
    }
    text_batch_count = 0;

    float atlas_w = (float)FONT_ATLAS_W;
    float atlas_h = (float)FONT_ATLAS_H;

    for (int row = 0; row < grid_rows; row++) {
        for (int col = 0; col < grid_cols; col++) {
            const char* str = cell_text[row][col];
            if (str[0] == '\0') continue;

            float x1, y1, x2, y2;
            cell_to_clip(row, col, grid_rows, grid_cols, &x1, &y1, &x2, &y2);

            float cw = x2 - x1;
            float ch = y2 - y1;

            int len = strlen(str);
            if (len > MAX_CELL_LEN) len = MAX_CELL_LEN;

            // Scale chars to fit cell: use ~70% of cell height, auto-width
            float char_h = ch * 0.65f;
            float char_w = char_h * (5.0f / 7.0f);

            // Center text horizontally and vertically
            float total_w = len * char_w * 0.85f;
            float start_x = x1 + (cw - total_w) * 0.5f;
            float start_y = y1 + (ch - char_h) * 0.5f;

            float cx = start_x;
            float cy = start_y;

            for (int i = 0; i < len; i++) {
                int ci = char_to_index(str[i]);
                if (ci < 0) { cx += char_w * 0.85f; continue; }

                int atlas_col_idx = ci % FONT_COLS;
                int atlas_row_idx = ci / FONT_COLS;

                float u0 = (atlas_col_idx * 6.0f) / atlas_w;
                float u1 = (atlas_col_idx * 6.0f + 5.0f) / atlas_w;
                float v0 = (atlas_row_idx * 8.0f) / atlas_h;
                float v1 = (atlas_row_idx * 8.0f + 7.0f) / atlas_h;

                int b = text_batch_count * 4;
                // Triangle 1
                text_batch[b +  0] = cx;          text_batch[b +  1] = cy;
                text_batch[b +  2] = u0;          text_batch[b +  3] = v1;
                text_batch[b +  4] = cx + char_w; text_batch[b +  5] = cy;
                text_batch[b +  6] = u1;          text_batch[b +  7] = v1;
                text_batch[b +  8] = cx + char_w; text_batch[b +  9] = cy + char_h;
                text_batch[b + 10] = u1;          text_batch[b + 11] = v0;
                // Triangle 2
                text_batch[b + 12] = cx;          text_batch[b + 13] = cy;
                text_batch[b + 14] = u0;          text_batch[b + 15] = v1;
                text_batch[b + 16] = cx + char_w; text_batch[b + 17] = cy + char_h;
                text_batch[b + 18] = u1;          text_batch[b + 19] = v0;
                text_batch[b + 20] = cx;          text_batch[b + 21] = cy + char_h;
                text_batch[b + 22] = u0;          text_batch[b + 23] = v0;

                text_batch_count += 6;
                cx += char_w * 0.85f;

                if (cx + char_w > x2) break;
            }
        }
    }

    if (text_batch_count == 0) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(text_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glUniform1i(glGetUniformLocation(text_program, "u_texture"), 0);
    glUniform3f(glGetUniformLocation(text_program, "u_color"), 1.0f, 1.0f, 1.0f);

    if (!text_vbo) glGenBuffers(1, &text_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glBufferData(GL_ARRAY_BUFFER, text_batch_count * 4 * sizeof(float), text_batch, GL_DYNAMIC_DRAW);

    GLint a_pos = glGetAttribLocation(text_program, "a_position");
    GLint a_uv = glGetAttribLocation(text_program, "a_uv");
    glEnableVertexAttribArray(a_pos);
    glEnableVertexAttribArray(a_uv);
    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glVertexAttribPointer(a_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLES, 0, text_batch_count);
    glDisableVertexAttribArray(a_pos);
    glDisableVertexAttribArray(a_uv);

    glDisable(GL_BLEND);
}

EMSCRIPTEN_KEEPALIVE
void set_cell_text(int row, int col, const char* text) {
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS || !text) return;
    strncpy(cell_text[row][col], text, MAX_CELL_LEN - 1);
    cell_text[row][col][MAX_CELL_LEN - 1] = '\0';
}

// ============================================================
// CURSOR - thin vertical bar drawn in the editing cell
// ============================================================

static int cursor_row = -1;
static int cursor_col = -1;
static int cursor_pos = 0;
static int cursor_visible = 0;

EMSCRIPTEN_KEEPALIVE
void set_cursor(int row, int col, int pos, int visible) {
    cursor_row = row;
    cursor_col = col;
    cursor_pos = pos;
    cursor_visible = visible;
}

static GLuint cursor_vbo = 0;

static void render_cursor(void) {
    if (!cursor_visible || cursor_row < 0 || cursor_col < 0) return;
    if (!grid_program) return;

    float x1, y1, x2, y2;
    cell_to_clip(cursor_row, cursor_col, grid_rows, grid_cols, &x1, &y1, &x2, &y2);

    float cw = x2 - x1;
    float ch = y2 - y1;

    // Match text layout from render_text
    const char* str = cell_text[cursor_row][cursor_col];
    int len = strlen(str);
    float char_h = ch * 0.65f;
    float char_w = char_h * (5.0f / 7.0f);
    float total_w = len * char_w * 0.85f;
    float start_x = x1 + (cw - total_w) * 0.5f;
    float start_y = y1 + (ch - char_h) * 0.5f;

    float cx = start_x + cursor_pos * char_w * 0.85f;
    float bar_w = char_w * 0.15f;

    float verts[] = {
        cx,         start_y,              1, 1, 1,
        cx + bar_w, start_y,              1, 1, 1,
        cx + bar_w, start_y + char_h,     1, 1, 1,
        cx,         start_y,              1, 1, 1,
        cx + bar_w, start_y + char_h,     1, 1, 1,
        cx,         start_y + char_h,     1, 1, 1,
    };

    glUseProgram(grid_program);
    if (!cursor_vbo) glGenBuffers(1, &cursor_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, cursor_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    GLint a_pos = glGetAttribLocation(grid_program, "a_position");
    GLint a_col = glGetAttribLocation(grid_program, "a_color");
    glEnableVertexAttribArray(a_pos);
    glEnableVertexAttribArray(a_col);
    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glVertexAttribPointer(a_col, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(a_pos);
    glDisableVertexAttribArray(a_col);
}

// ============================================================
// RENDER (backgrounds + text + cursor in one call)
// ============================================================

EMSCRIPTEN_KEEPALIVE
void render_grid(void) {
    ensure_context();

    // Defensively reset GL state to prevent cross-call contamination.
    // JS event handlers (click, blink timer, price tick) can all invoke
    // render_grid at arbitrary points; stale attrib/buffer bindings from
    // a prior call or from update_grid_buffer cause the "prism" artefact.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);
    glDisable(GL_BLEND);

    glClearColor(0.08f, 0.08f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    render_grid_bg();
    render_text();
    render_cursor();
}

// ============================================================
// HIT TESTING (canvas click → cell coordinates)
// ============================================================

EMSCRIPTEN_KEEPALIVE
int get_cell_at(float clip_x, float clip_y) {
    if (grid_rows <= 0 || grid_cols <= 0) return -1;
    float cell_w = 2.0f / grid_cols;
    float cell_h = 2.0f / grid_rows;
    int col = (int)((clip_x + 1.0f) / cell_w);
    int row = (int)((1.0f - clip_y) / cell_h);
    if (col >= 0 && col < grid_cols && row >= 0 && row < grid_rows)
        return row * 256 + col;
    return -1;
}
