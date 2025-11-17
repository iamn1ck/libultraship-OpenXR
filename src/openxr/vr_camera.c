#include "vr_camera.h"
#include "openxr_manager.h"

#include <math.h>
#include <stdio.h>

static int vr_camera_initialized = 0;
static int vr_camera_active = 0;

// Camera state
static struct {
    float yaw;
    float pitch;
    float roll;
    float x;
    float y;
    float z;
} vr_camera_state = {0};

void vr_camera_init(void)
{
    vr_camera_initialized = 1;
    vr_camera_active = openxr_is_initialized();
}

void vr_camera_update(void)
{
    if (!vr_camera_initialized || !openxr_is_initialized()) {
        vr_camera_active = 0;
        return;
    }

    // Get head rotation from OpenXR
    if (!openxr_get_head_rotation(&vr_camera_state.yaw, &vr_camera_state.pitch, &vr_camera_state.roll)) {
        vr_camera_active = 0;
        return;
    }

    // Get head position from OpenXR
    if (!openxr_get_head_position(&vr_camera_state.x, &vr_camera_state.y, &vr_camera_state.z)) {
        vr_camera_active = 0;
        return;
    }

    vr_camera_active = 1;

    // Debug logging - print VR headset data
    static int log_counter = 0;
    if (log_counter % 60 == 0) {  // Log once per second (at 60 fps)
        printf("\n=== VR HEADSET DATA ===\n");
        printf("Rotation: Yaw=%.2f° Pitch=%.2f° Roll=%.2f°\n", 
               vr_camera_state.yaw, vr_camera_state.pitch, vr_camera_state.roll);
        printf("Position: X=%.3fm Y=%.3fm Z=%.3fm\n", 
               vr_camera_state.x, vr_camera_state.y, vr_camera_state.z);
    }
    
    log_counter++;
}

int vr_camera_is_active(void)
{
    return vr_camera_active;
}

