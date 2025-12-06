#ifndef VR_COPY_H
#define VR_COPY_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialize VR copy system (OpenGL-Vulkan interop)
// This creates shared memory objects for efficient frame transfer
// Returns 1 on success, 0 on failure
int vr_copy_init(void);

// Shutdown VR copy system
void vr_copy_shutdown(void);

// Check if VR copy is initialized
int vr_copy_is_initialized(void);

// Copy framebuffer to swapchain for an eye
// Returns 1 on success, 0 on failure
int vr_copy_framebuffer_to_swapchain(int eye);

// Copy quad framebuffer to quad swapchain
// Returns 1 on success, 0 on failure
int vr_copy_quad_framebuffer_to_swapchain(void);

// Copy quad 2 framebuffer to quad 2 swapchain
// Returns 1 on success, 0 on failure
int vr_copy_quad2_framebuffer_to_swapchain(void);

#ifdef __cplusplus
}
#endif

#endif // VR_COPY_H
