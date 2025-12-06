#ifndef VR_CAMERA_H
#define VR_CAMERA_H

#include "libultraship/libultra/gbi.h"


#ifdef __cplusplus
extern "C" {
#endif

struct Camera;

// Initialize VR camera system
void vr_camera_init(void);

// Update camera with VR head tracking
// Should be called each frame before camera updates
void vr_camera_update(void);

// Check if VR camera is active
int vr_camera_is_active(void);

#ifdef __cplusplus
}
#endif

#endif // VR_CAMERA_H

