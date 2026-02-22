// WebGL Basics: GPU-Accelerated Rendering from C
//
// WebGL uses the GPU pipeline:
// 1. Vertex Shader   - runs once per vertex, positions geometry
// 2. Rasterization   - GPU converts triangles to pixels
// 3. Fragment Shader - runs once per pixel, determines color
//
// Emscripten maps OpenGL ES 2.0/3.0 calls to WebGL automatically!

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

// Shader program
static GLuint program = 0;
static GLuint vbo = 0;

// Uniform locations (variables we pass to shaders)
static GLint u_time = -1;
static GLint u_resolution = -1;
static GLint u_color = -1;

// Canvas size
static int canvas_width = 800;
static int canvas_height = 600;

// Animation time
static float time_elapsed = 0.0f;

// Vertex shader - transforms vertex positions
// gl_Position is the output (clip space coordinates)
static const char* vertex_shader_src = 
    "attribute vec2 a_position;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

// Fragment shader - determines pixel color
// gl_FragColor is the output (RGBA)
static const char* fragment_shader_src =
    "precision mediump float;\n"
    "uniform float u_time;\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec3 u_color;\n"
    "void main() {\n"
    "    vec2 uv = gl_FragCoord.xy / u_resolution;\n"
    "    float pulse = sin(u_time * 3.0) * 0.2 + 0.8;\n"
    "    gl_FragColor = vec4(u_color * pulse, 1.0);\n"
    "}\n";

// Compile a shader and check for errors
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

// Link shaders into a program
static GLuint create_program(const char* vert_src, const char* frag_src) {
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    
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

// Initialize WebGL
EMSCRIPTEN_KEEPALIVE
int init_webgl(int width, int height) {
    canvas_width = width;
    canvas_height = height;
    
    // Create WebGL context
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha = 0;
    attrs.depth = 0;
    attrs.antialias = 1;
    attrs.majorVersion = 1;  // WebGL 1.0 (OpenGL ES 2.0)
    
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#webgl-canvas", &attrs);
    if (ctx <= 0) {
        printf("Failed to create WebGL context\n");
        return 0;
    }
    emscripten_webgl_make_context_current(ctx);
    
    // Create shader program
    program = create_program(vertex_shader_src, fragment_shader_src);
    if (!program) return 0;
    
    // Get uniform locations
    u_time = glGetUniformLocation(program, "u_time");
    u_resolution = glGetUniformLocation(program, "u_resolution");
    u_color = glGetUniformLocation(program, "u_color");
    
    // Create a simple quad (two triangles)
    // Coordinates are in "clip space": -1 to 1
    float vertices[] = {
        // Triangle 1
        -0.8f, -0.6f,
         0.8f, -0.6f,
         0.8f,  0.6f,
        // Triangle 2
        -0.8f, -0.6f,
         0.8f,  0.6f,
        -0.8f,  0.6f
    };
    
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Set up vertex attribute
    GLint a_position = glGetAttribLocation(program, "a_position");
    glEnableVertexAttribArray(a_position);
    glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE, 0, 0);
    
    glViewport(0, 0, width, height);
    
    printf("WebGL initialized: %dx%d\n", width, height);
    return 1;
}

// Render a frame
EMSCRIPTEN_KEEPALIVE
void render_frame(float delta_time) {
    time_elapsed += delta_time;
    
    // Clear to dark background
    glClearColor(0.1f, 0.1f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(program);
    
    // Set uniforms
    glUniform1f(u_time, time_elapsed);
    glUniform2f(u_resolution, (float)canvas_width, (float)canvas_height);
    glUniform3f(u_color, 0.0f, 0.83f, 1.0f);  // Cyan
    
    // Draw the quad (6 vertices = 2 triangles)
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ============================================================
// TABLE GRID RENDERING - The foundation for your data table!
// ============================================================

// Grid shader - each cell can have its own color
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

// Convert row,col to clip space coordinates
static void cell_to_clip(int row, int col, int total_rows, int total_cols,
                         float* x1, float* y1, float* x2, float* y2) {
    float cell_w = 2.0f / total_cols;
    float cell_h = 2.0f / total_rows;
    
    *x1 = -1.0f + col * cell_w + 0.01f;       // Small padding
    *x2 = -1.0f + (col + 1) * cell_w - 0.01f;
    *y1 = 1.0f - (row + 1) * cell_h + 0.01f;  // Y is flipped
    *y2 = 1.0f - row * cell_h - 0.01f;
}

// Initialize grid rendering
EMSCRIPTEN_KEEPALIVE
int init_grid(int rows, int cols) {
    if (!grid_program) {
        grid_program = create_program(grid_vertex_src, grid_fragment_src);
        if (!grid_program) return 0;
    }
    
    // Each cell = 2 triangles = 6 vertices
    // Each vertex = x, y, r, g, b (5 floats)
    int cells = rows * cols;
    grid_vertex_count = cells * 6;
    int floats_needed = grid_vertex_count * 5;
    
    if (grid_vertices) free(grid_vertices);
    grid_vertices = (float*)malloc(floats_needed * sizeof(float));
    
    // Generate grid geometry
    int idx = 0;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            float x1, y1, x2, y2;
            cell_to_clip(row, col, rows, cols, &x1, &y1, &x2, &y2);
            
            // Default colors: header row cyan, alternating body rows
            float r, g, b;
            if (row == 0) {
                r = 0.0f; g = 0.5f; b = 0.7f;  // Header
            } else if (row % 2 == 0) {
                r = 0.15f; g = 0.15f; b = 0.25f;
            } else {
                r = 0.2f; g = 0.2f; b = 0.32f;
            }
            
            // Triangle 1
            grid_vertices[idx++] = x1; grid_vertices[idx++] = y1;
            grid_vertices[idx++] = r;  grid_vertices[idx++] = g; grid_vertices[idx++] = b;
            
            grid_vertices[idx++] = x2; grid_vertices[idx++] = y1;
            grid_vertices[idx++] = r;  grid_vertices[idx++] = g; grid_vertices[idx++] = b;
            
            grid_vertices[idx++] = x2; grid_vertices[idx++] = y2;
            grid_vertices[idx++] = r;  grid_vertices[idx++] = g; grid_vertices[idx++] = b;
            
            // Triangle 2
            grid_vertices[idx++] = x1; grid_vertices[idx++] = y1;
            grid_vertices[idx++] = r;  grid_vertices[idx++] = g; grid_vertices[idx++] = b;
            
            grid_vertices[idx++] = x2; grid_vertices[idx++] = y2;
            grid_vertices[idx++] = r;  grid_vertices[idx++] = g; grid_vertices[idx++] = b;
            
            grid_vertices[idx++] = x1; grid_vertices[idx++] = y2;
            grid_vertices[idx++] = r;  grid_vertices[idx++] = g; grid_vertices[idx++] = b;
        }
    }
    
    // Upload to GPU
    if (!grid_vbo) glGenBuffers(1, &grid_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, floats_needed * sizeof(float), grid_vertices, GL_DYNAMIC_DRAW);
    
    return 1;
}

// Update a single cell's color (for highlighting, selection, etc.)
EMSCRIPTEN_KEEPALIVE
void set_cell_color(int row, int col, int total_cols, float r, float g, float b) {
    if (!grid_vertices) return;
    
    int cell_idx = row * total_cols + col;
    int base = cell_idx * 6 * 5;  // 6 vertices * 5 floats each
    
    // Update color for all 6 vertices of this cell
    for (int v = 0; v < 6; v++) {
        int offset = base + v * 5 + 2;  // +2 to skip x,y
        grid_vertices[offset + 0] = r;
        grid_vertices[offset + 1] = g;
        grid_vertices[offset + 2] = b;
    }
}

// Upload changed vertex data to GPU
EMSCRIPTEN_KEEPALIVE
void update_grid_buffer() {
    if (!grid_vertices || !grid_vbo) return;
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, grid_vertex_count * 5 * sizeof(float), grid_vertices);
}

// Render the grid
EMSCRIPTEN_KEEPALIVE
void render_grid() {
    glClearColor(0.08f, 0.08f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(grid_program);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    
    GLint a_pos = glGetAttribLocation(grid_program, "a_position");
    GLint a_col = glGetAttribLocation(grid_program, "a_color");
    
    glEnableVertexAttribArray(a_pos);
    glEnableVertexAttribArray(a_col);
    
    // Stride = 5 floats (x, y, r, g, b)
    glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glVertexAttribPointer(a_col, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glDrawArrays(GL_TRIANGLES, 0, grid_vertex_count);
}

// Get vertex buffer pointer for JS to read cell positions (for DOM text overlay)
EMSCRIPTEN_KEEPALIVE
float* get_grid_vertices() {
    return grid_vertices;
}

EMSCRIPTEN_KEEPALIVE
int get_grid_vertex_count() {
    return grid_vertex_count;
}
