// Canvas Rendering: C writes pixels, JavaScript displays them
//
// This demonstrates the pattern you'll use for high-performance rendering:
// 1. C allocates and owns a pixel buffer
// 2. C writes RGBA pixel data to the buffer
// 3. JavaScript reads the buffer and draws to canvas
// 4. Repeat for animation

#include <emscripten.h>
#include <stdlib.h>
#include <math.h>

// Pixel buffer - RGBA format (4 bytes per pixel)
static unsigned char* pixel_buffer = NULL;
static int buffer_width = 0;
static int buffer_height = 0;

// Animation state
static float time = 0.0f;

// Initialize the pixel buffer
EMSCRIPTEN_KEEPALIVE
unsigned char* init_buffer(int width, int height) {
    if (pixel_buffer) {
        free(pixel_buffer);
    }
    
    buffer_width = width;
    buffer_height = height;
    pixel_buffer = (unsigned char*)malloc(width * height * 4);
    
    return pixel_buffer;
}

// Get buffer pointer (for JS to read)
EMSCRIPTEN_KEEPALIVE
unsigned char* get_buffer() {
    return pixel_buffer;
}

EMSCRIPTEN_KEEPALIVE
int get_buffer_width() {
    return buffer_width;
}

EMSCRIPTEN_KEEPALIVE
int get_buffer_height() {
    return buffer_height;
}

// Helper: Set a pixel's RGBA values
static void set_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (x < 0 || x >= buffer_width || y < 0 || y >= buffer_height) return;
    
    int index = (y * buffer_width + x) * 4;
    pixel_buffer[index + 0] = r;
    pixel_buffer[index + 1] = g;
    pixel_buffer[index + 2] = b;
    pixel_buffer[index + 3] = a;
}

// Clear the buffer to a solid color
EMSCRIPTEN_KEEPALIVE
void clear_buffer(unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; i < buffer_width * buffer_height * 4; i += 4) {
        pixel_buffer[i + 0] = r;
        pixel_buffer[i + 1] = g;
        pixel_buffer[i + 2] = b;
        pixel_buffer[i + 3] = 255;
    }
}

// Draw a filled rectangle
EMSCRIPTEN_KEEPALIVE
void draw_rect(int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b) {
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            set_pixel(px, py, r, g, b, 255);
        }
    }
}

// Draw a horizontal line (useful for table grid)
EMSCRIPTEN_KEEPALIVE
void draw_hline(int x, int y, int length, unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; i < length; i++) {
        set_pixel(x + i, y, r, g, b, 255);
    }
}

// Draw a vertical line
EMSCRIPTEN_KEEPALIVE
void draw_vline(int x, int y, int length, unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; i < length; i++) {
        set_pixel(x, y + i, r, g, b, 255);
    }
}

// Demo: Render a simple animated pattern
EMSCRIPTEN_KEEPALIVE
void render_demo_frame(float delta_time) {
    time += delta_time;
    
    // Clear to dark background
    clear_buffer(26, 26, 46);
    
    // Animated bars - like a simple table with moving highlights
    int row_height = 30;
    int num_rows = buffer_height / row_height;
    
    for (int row = 0; row < num_rows; row++) {
        int y = row * row_height;
        
        // Alternating row colors
        if (row % 2 == 0) {
            draw_rect(0, y, buffer_width, row_height - 1, 34, 34, 60);
        }
        
        // Animated highlight that moves across rows
        float phase = time * 2.0f + row * 0.3f;
        int highlight_x = (int)((sinf(phase) * 0.5f + 0.5f) * (buffer_width - 100));
        draw_rect(highlight_x, y + 5, 80, row_height - 11, 0, 212, 255);
        
        // Row separator line
        draw_hline(0, y + row_height - 1, buffer_width, 60, 60, 80);
    }
}

// Demo: Render a static grid (like a table structure)
EMSCRIPTEN_KEEPALIVE
void render_table_grid(int rows, int cols) {
    clear_buffer(20, 20, 35);
    
    int cell_width = buffer_width / cols;
    int cell_height = buffer_height / rows;
    
    // Draw cells with alternating colors
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int x = col * cell_width;
            int y = row * cell_height;
            
            // Header row
            if (row == 0) {
                draw_rect(x + 1, y + 1, cell_width - 2, cell_height - 2, 0, 100, 150);
            }
            // Alternating body rows
            else if (row % 2 == 0) {
                draw_rect(x + 1, y + 1, cell_width - 2, cell_height - 2, 30, 30, 50);
            } else {
                draw_rect(x + 1, y + 1, cell_width - 2, cell_height - 2, 40, 40, 65);
            }
        }
    }
    
    // Draw grid lines
    for (int col = 0; col <= cols; col++) {
        draw_vline(col * cell_width, 0, buffer_height, 70, 70, 100);
    }
    for (int row = 0; row <= rows; row++) {
        draw_hline(0, row * cell_height, buffer_width, 70, 70, 100);
    }
}

// Cleanup
EMSCRIPTEN_KEEPALIVE
void destroy_buffer() {
    if (pixel_buffer) {
        free(pixel_buffer);
        pixel_buffer = NULL;
    }
    buffer_width = 0;
    buffer_height = 0;
}
