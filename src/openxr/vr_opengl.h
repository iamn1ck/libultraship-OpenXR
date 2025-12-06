#ifndef VR_OPENGL_H
#define VR_OPENGL_H

#include <stdint.h>

// #ifdef __cplusplus
// extern "C" {
// #endif

// Initialize VR OpenGL integration
// Creates framebuffers and textures for VR rendering
// Returns 1 on success, 0 on failure
int vr_opengl_init(void);

// Shutdown VR OpenGL integration
void vr_opengl_shutdown(void);

// Check if VR OpenGL is initialized
int vr_opengl_is_initialized(void);

// Begin rendering to an eye
// eye: 0 for left, 1 for right
// Binds the framebuffer for that eye
// Returns 1 on success, 0 on failure
int vr_opengl_begin_eye(int eye);

// End rendering to an eye
// Unbinds the framebuffer
void vr_opengl_end_eye(int eye);

// Get the framebuffer ID for an eye (for debugging)
unsigned int vr_opengl_get_framebuffer(int eye);

// Get the texture ID for an eye (for debugging/copying)
unsigned int vr_opengl_get_texture(int eye);

// Get the viewport dimensions for an eye
void vr_opengl_get_viewport(int eye, uint32_t* width, uint32_t* height);

// Initialize VR OpenGL for quad layer
// Creates framebuffer and texture for quad rendering
// Returns 1 on success, 0 on failure
int vr_opengl_init_quad(uint32_t width, uint32_t height);

// Begin rendering to quad layer
// Binds the quad framebuffer
// Returns 1 on success, 0 on failure
int vr_opengl_begin_quad(void);

// End rendering to quad layer
// Unbinds the framebuffer
void vr_opengl_end_quad(void);
void vr_opengl_cancel_quad(void); // Unbinds without copying

// Get the quad framebuffer ID
unsigned int vr_opengl_get_quad_framebuffer(void);

// Get the quad texture ID
unsigned int vr_opengl_get_quad_texture(void);

// Quad layer 2 functions
int vr_opengl_init_quad2(uint32_t width, uint32_t height);
int vr_opengl_begin_quad2(void);
void vr_opengl_end_quad2(void);
void vr_opengl_cancel_quad2(void);
unsigned int vr_opengl_get_quad_framebuffer2(void);
unsigned int vr_opengl_get_quad_texture2(void);

// Helper to draw a hello triangle
void vr_opengl_draw_hello_triangle(void);

// #ifdef __cplusplus
// }
// #endif

#endif // VR_OPENGL_H

