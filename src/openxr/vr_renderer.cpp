#include "vr_renderer.h"
#include "openxr_manager.h"
#include "openxr_swapchain.h"

#include <iostream>
#include <cstring>
#include <cmath>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <spdlog/spdlog.h>

using namespace std;

// Forward declarations from openxr_manager (declared in header with C++ linkage)
// These are already declared in openxr_manager.h, no need to redeclare

// VR renderer state
static struct {
    bool initialized;
    OpenXRSwapchain* leftSwapchain;
    OpenXRSwapchain* rightSwapchain;
    
    // OpenXR handles (from manager)
    XrInstance xrInstance;
    XrSession xrSession;
    XrSpace xrSpace;
    XrSystemId xrSystemId;
    
    // Frame state
    XrFrameState frameState;
    bool frameActive;
    
    // View state
    XrView views[2];
    bool viewsValid;
    
    // Swapchain image indices
    uint32_t swapchainIndices[2];

    // Quad layer state
    OpenXRSwapchain* quadSwapchain;
    bool quadLayerInitialized;
    bool quadLayerActive;
    uint32_t quadSwapchainIndex;
    struct {
        XrPosef pose;
        XrExtent2Df size;
    } quadLayer;
} g_vr_renderer = {
    false,
    nullptr,
    nullptr,
    XR_NULL_HANDLE,
    XR_NULL_HANDLE,
    XR_NULL_HANDLE,
    XR_NULL_SYSTEM_ID,
    {},
    false,
    {},
    false,
    {0, 0},
    nullptr,
    false,
    false,
    0,
    {
        {{0, 0, 0, 1}, {0, 0, -1}}, // Default pose: 1m in front
        {1.0f, 1.0f}                // Default size: 1x1m
    }
};

int vr_renderer_init(void)
{
    if (g_vr_renderer.initialized) {
        cout << "VR renderer already initialized" << endl;
        return 1;
    }
    
    if (!openxr_is_initialized()) {
        cerr << "Cannot initialize VR renderer: OpenXR not initialized" << endl;
        return 0;
    }
    
    cout << "Initializing VR renderer..." << endl;
    
    // Get OpenXR handles from manager
    g_vr_renderer.xrInstance = openxr_get_instance();
    g_vr_renderer.xrSession = openxr_get_session();
    g_vr_renderer.xrSpace = openxr_get_space();
    g_vr_renderer.xrSystemId = openxr_get_system_id();
    
    if (g_vr_renderer.xrInstance == XR_NULL_HANDLE ||
        g_vr_renderer.xrSession == XR_NULL_HANDLE ||
        g_vr_renderer.xrSpace == XR_NULL_HANDLE ||
        g_vr_renderer.xrSystemId == XR_NULL_SYSTEM_ID) {
        cerr << "Failed to get OpenXR handles from manager" << endl;
        return 0;
    }
    
    // Create swapchains
    if (!createOpenXRSwapchains(
            g_vr_renderer.xrInstance,
            g_vr_renderer.xrSystemId,
            g_vr_renderer.xrSession,
            &g_vr_renderer.leftSwapchain,
            &g_vr_renderer.rightSwapchain)) {
        cerr << "Failed to create OpenXR swapchains" << endl;
        return 0;
    }
    
    cout << "VR renderer initialized successfully" << endl;
    cout << "Left eye: " << g_vr_renderer.leftSwapchain->width 
         << "x" << g_vr_renderer.leftSwapchain->height << endl;
    cout << "Right eye: " << g_vr_renderer.rightSwapchain->width 
         << "x" << g_vr_renderer.rightSwapchain->height << endl;

    SPDLOG_WARN(" g_vr_renderer.initialized intialized");

    
    g_vr_renderer.initialized = true;
    return 1;
}

void vr_renderer_shutdown(void)
{
    if (!g_vr_renderer.initialized) {
        return;
    }
    
    cout << "Shutting down VR renderer..." << endl;
    
    destroyOpenXRSwapchain(g_vr_renderer.leftSwapchain);
    destroyOpenXRSwapchain(g_vr_renderer.rightSwapchain);
    if (g_vr_renderer.quadLayerInitialized) {
        destroyOpenXRSwapchain(g_vr_renderer.quadSwapchain);
        g_vr_renderer.quadSwapchain = nullptr;
        g_vr_renderer.quadLayerInitialized = false;
    }
    
    g_vr_renderer.leftSwapchain = nullptr;
    g_vr_renderer.rightSwapchain = nullptr;
    g_vr_renderer.initialized = false;
    
    cout << "VR renderer shutdown complete" << endl;
}

int vr_renderer_is_initialized(void)
{
    return g_vr_renderer.initialized ? 1 : 0;
}

int vr_renderer_begin_frame(void)
{
    if (!g_vr_renderer.initialized) {
        SPDLOG_WARN(" vr_renderer_begin_frame not initied");

        return 0;
    }
    
    // Frame state was already updated by openxr_update()
    // Just mark that we're starting a frame
    g_vr_renderer.frameActive = true;
    
    // Locate views to get eye poses
    XrViewState viewState{};
    viewState.type = XR_TYPE_VIEW_STATE;
    
    uint32_t viewCount = 2;
    g_vr_renderer.views[0].type = XR_TYPE_VIEW;
    g_vr_renderer.views[0].next = nullptr;
    g_vr_renderer.views[1].type = XR_TYPE_VIEW;
    g_vr_renderer.views[1].next = nullptr;
    
    XrViewLocateInfo viewLocateInfo{};
    viewLocateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = g_vr_renderer.frameState.predictedDisplayTime;
    viewLocateInfo.space = g_vr_renderer.xrSpace;
    
    XrResult result = xrLocateViews(
        g_vr_renderer.xrSession,
        &viewLocateInfo,
        &viewState,
        viewCount,
        &viewCount,
        g_vr_renderer.views
    );
    
    if (result != XR_SUCCESS || viewCount != 2) {
            SPDLOG_WARN(" Failed to locate views:");

        cerr << "Failed to locate views: " << result << endl;
        g_vr_renderer.viewsValid = false;
        return 0;
    }
    
    g_vr_renderer.viewsValid = true;
    return 1;
}

int vr_renderer_render_eye(int eye)
{
    if (!g_vr_renderer.initialized || !g_vr_renderer.frameActive || !g_vr_renderer.viewsValid) {
        return 0;
    }
    
    if (eye < 0 || eye > 1) {
        cerr << "Invalid eye index: " << eye << endl;
        return 0;
    }
    
    OpenXRSwapchain* swapchain = (eye == 0) ? g_vr_renderer.leftSwapchain : g_vr_renderer.rightSwapchain;
    
    // Acquire swapchain image
    XrSwapchainImageAcquireInfo acquireInfo{};
    acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    
    uint32_t imageIndex = 0;
    XrResult result = xrAcquireSwapchainImage(swapchain->swapchain, &acquireInfo, &imageIndex);
    
    if (result != XR_SUCCESS) {
        cerr << "Failed to acquire swapchain image for eye " << eye << ": " << result << endl;
        return 0;
    }
    
    g_vr_renderer.swapchainIndices[eye] = imageIndex;
    
    // Wait for swapchain image
    XrSwapchainImageWaitInfo waitInfo{};
    waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    waitInfo.timeout = XR_INFINITE_DURATION;
    
    result = xrWaitSwapchainImage(swapchain->swapchain, &waitInfo);
    
    if (result != XR_SUCCESS) {
        cerr << "Failed to wait for swapchain image for eye " << eye << ": " << result << endl;
        return 0;
    }
    
    // At this point, the game should render to swapchain->images[imageIndex]
    // This will be handled by the OpenGL integration
    
    return 1;
}

int vr_renderer_end_frame(void)
{
    if (!g_vr_renderer.initialized || !g_vr_renderer.frameActive) {
        return 0;
    }
    
    // Release swapchain images
    for (int eye = 0; eye < 2; eye++) {
        OpenXRSwapchain* swapchain = (eye == 0) ? g_vr_renderer.leftSwapchain : g_vr_renderer.rightSwapchain;
        
        XrSwapchainImageReleaseInfo releaseInfo{};
        releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
        
        XrResult result = xrReleaseSwapchainImage(swapchain->swapchain, &releaseInfo);
        
        if (result != XR_SUCCESS) {
            cerr << "Failed to release swapchain image for eye " << eye << ": " << result << endl;
        }
    }
    
    // Release quad layer swapchain image if active
    if (g_vr_renderer.quadLayerActive) {
        XrSwapchainImageReleaseInfo releaseInfo{};
        releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
        
        XrResult result = xrReleaseSwapchainImage(g_vr_renderer.quadSwapchain->swapchain, &releaseInfo);
        
        if (result != XR_SUCCESS) {
            cerr << "Failed to release quad swapchain image: " << result << endl;
        }
    }
    
    // Submit frame to OpenXR
    XrCompositionLayerProjectionView projectionViews[2]{};
    
    for (int eye = 0; eye < 2; eye++) {
        OpenXRSwapchain* swapchain = (eye == 0) ? g_vr_renderer.leftSwapchain : g_vr_renderer.rightSwapchain;
        
        projectionViews[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        projectionViews[eye].pose = g_vr_renderer.views[eye].pose;
        projectionViews[eye].fov = g_vr_renderer.views[eye].fov;
        projectionViews[eye].subImage.swapchain = swapchain->swapchain;
        projectionViews[eye].subImage.imageRect.offset = {0, 0};
        projectionViews[eye].subImage.imageRect.extent = {(int32_t)swapchain->width, (int32_t)swapchain->height};
        projectionViews[eye].subImage.imageArrayIndex = 0;
    }
    
    XrCompositionLayerProjection layer{};
    layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    layer.space = g_vr_renderer.xrSpace;
    layer.viewCount = 2;
    layer.views = projectionViews;
    
    // Quad layer
    XrCompositionLayerQuad quadLayer{};
    quadLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    quadLayer.space = g_vr_renderer.xrSpace;
    quadLayer.subImage.swapchain = g_vr_renderer.quadSwapchain ? g_vr_renderer.quadSwapchain->swapchain : XR_NULL_HANDLE;
    quadLayer.subImage.imageRect.offset = {0, 0};
    if (g_vr_renderer.quadSwapchain) {
        quadLayer.subImage.imageRect.extent = {(int32_t)g_vr_renderer.quadSwapchain->width, (int32_t)g_vr_renderer.quadSwapchain->height};
    }
    quadLayer.subImage.imageArrayIndex = 0;
    quadLayer.pose = g_vr_renderer.quadLayer.pose;
    quadLayer.size = g_vr_renderer.quadLayer.size;
    quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    
    std::vector<const XrCompositionLayerBaseHeader*> layers;
    layers.push_back((const XrCompositionLayerBaseHeader*)&layer);
    
    if (g_vr_renderer.quadLayerActive) {
        layers.push_back((const XrCompositionLayerBaseHeader*)&quadLayer);
    }
    
    XrFrameEndInfo frameEndInfo{};
    frameEndInfo.type = XR_TYPE_FRAME_END_INFO;
    frameEndInfo.displayTime = g_vr_renderer.frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    frameEndInfo.layerCount = (uint32_t)layers.size();
    frameEndInfo.layers = layers.data();
    
    XrResult result = xrEndFrame(g_vr_renderer.xrSession, &frameEndInfo);
    
    if (result != XR_SUCCESS) {
        cerr << "Failed to end OpenXR frame: " << result << endl;
        g_vr_renderer.frameActive = false;
        g_vr_renderer.quadLayerActive = false;
        return 0;
    }
    
    g_vr_renderer.frameActive = false;
    g_vr_renderer.quadLayerActive = false;
    return 1;
}

void vr_renderer_get_viewport(int eye, uint32_t* width, uint32_t* height)
{
    if (!g_vr_renderer.initialized || eye < 0 || eye > 1) {
        *width = 0;
        *height = 0;
        return;
    }
    
    OpenXRSwapchain* swapchain = (eye == 0) ? g_vr_renderer.leftSwapchain : g_vr_renderer.rightSwapchain;
    *width = swapchain->width;
    *height = swapchain->height;
}

// Helper function to convert XrFovf to projection matrix
static void fov_to_projection_matrix(const XrFovf& fov, float nearZ, float farZ, float* matrix)
{
    const float tanLeft = tanf(fov.angleLeft);
    const float tanRight = tanf(fov.angleRight);
    const float tanDown = tanf(fov.angleDown);
    const float tanUp = tanf(fov.angleUp);
    
    const float tanWidth = tanRight - tanLeft;
    const float tanHeight = tanUp - tanDown;
    
    // Standard OpenGL projection matrix
    memset(matrix, 0, 16 * sizeof(float));
    
    matrix[0] = 2.0f / tanWidth;
    matrix[4] = 0.0f;
    matrix[8] = (tanRight + tanLeft) / tanWidth;
    matrix[12] = 0.0f;
    
    matrix[1] = 0.0f;
    matrix[5] = 2.0f / tanHeight;
    matrix[9] = (tanUp + tanDown) / tanHeight;
    matrix[13] = 0.0f;
    
    matrix[2] = 0.0f;
    matrix[6] = 0.0f;
    matrix[10] = -(farZ + nearZ) / (farZ - nearZ);
    matrix[14] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    
    matrix[3] = 0.0f;
    matrix[7] = 0.0f;
    matrix[11] = -1.0f;
    matrix[15] = 0.0f;
}

int vr_renderer_get_projection_matrix(int eye, float* matrix)
{
    if (!g_vr_renderer.initialized || !g_vr_renderer.viewsValid || eye < 0 || eye > 1) {
        return 0;
    }
    
    // Convert OpenXR FOV to projection matrix
    // Use typical near/far plane values for SM64
    fov_to_projection_matrix(g_vr_renderer.views[eye].fov, 100.0f, 32000.0f, matrix);
    
    return 1;
}

// Helper function to convert XrPosef to view matrix
static void pose_to_view_matrix(const XrPosef& pose, float* matrix)
{
    // Convert quaternion to rotation matrix
    const XrQuaternionf& q = pose.orientation;
    const XrVector3f& p = pose.position;
    
    // Create rotation matrix from quaternion
    float rotMatrix[16];
    memset(rotMatrix, 0, 16 * sizeof(float));
    
    rotMatrix[0] = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    rotMatrix[1] = 2.0f * (q.x * q.y + q.w * q.z);
    rotMatrix[2] = 2.0f * (q.x * q.z - q.w * q.y);
    rotMatrix[3] = 0.0f;
    
    rotMatrix[4] = 2.0f * (q.x * q.y - q.w * q.z);
    rotMatrix[5] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    rotMatrix[6] = 2.0f * (q.y * q.z + q.w * q.x);
    rotMatrix[7] = 0.0f;
    
    rotMatrix[8] = 2.0f * (q.x * q.z + q.w * q.y);
    rotMatrix[9] = 2.0f * (q.y * q.z - q.w * q.x);
    rotMatrix[10] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    rotMatrix[11] = 0.0f;
    
    rotMatrix[12] = 0.0f;
    rotMatrix[13] = 0.0f;
    rotMatrix[14] = 0.0f;
    rotMatrix[15] = 1.0f;
    
    // Invert the view matrix (view = inverse of pose)
    // For a rigid body transform, inverse is transpose of rotation and negated position
    matrix[0] = rotMatrix[0];
    matrix[1] = rotMatrix[4];
    matrix[2] = rotMatrix[8];
    matrix[3] = 0.0f;
    
    matrix[4] = rotMatrix[1];
    matrix[5] = rotMatrix[5];
    matrix[6] = rotMatrix[9];
    matrix[7] = 0.0f;
    
    matrix[8] = rotMatrix[2];
    matrix[9] = rotMatrix[6];
    matrix[10] = rotMatrix[10];
    matrix[11] = 0.0f;
    
    matrix[12] = -(rotMatrix[0] * p.x + rotMatrix[1] * p.y + rotMatrix[2] * p.z);
    matrix[13] = -(rotMatrix[4] * p.x + rotMatrix[5] * p.y + rotMatrix[6] * p.z);
    matrix[14] = -(rotMatrix[8] * p.x + rotMatrix[9] * p.y + rotMatrix[10] * p.z);
    matrix[15] = 1.0f;
}

int vr_renderer_get_view_matrix(int eye, float* matrix)
{
    if (!g_vr_renderer.initialized || !g_vr_renderer.viewsValid || eye < 0 || eye > 1) {
        return 0;
    }
    
    pose_to_view_matrix(g_vr_renderer.views[eye].pose, matrix);

    return 1;
}

// This function will be called by openxr_manager to update frame state
extern "C" void vr_renderer_set_frame_state(XrFrameState frameState)
{
    g_vr_renderer.frameState = frameState;
}

// Get the current swapchain image for an eye
VkImage vr_renderer_get_swapchain_image(int eye)
{
    if (!g_vr_renderer.initialized || eye < 0 || eye > 1) {
        return VK_NULL_HANDLE;
    }
    
    OpenXRSwapchain* swapchain = (eye == 0) ? g_vr_renderer.leftSwapchain : g_vr_renderer.rightSwapchain;
    if (!swapchain || !swapchain->images) {
        return VK_NULL_HANDLE;
    }
    
    uint32_t imageIndex = g_vr_renderer.swapchainIndices[eye];
    if (imageIndex >= swapchain->imageCount) {
        return VK_NULL_HANDLE;
    }
    
    return swapchain->images[imageIndex];
}

// Get the swapchain image format
uint32_t vr_renderer_get_swapchain_format(int eye)
{
    if (!g_vr_renderer.initialized || eye < 0 || eye > 1) {
        return 0;
    }
    
    OpenXRSwapchain* swapchain = (eye == 0) ? g_vr_renderer.leftSwapchain : g_vr_renderer.rightSwapchain;
    if (!swapchain) {
        return 0;
    }
    
    return (uint32_t)swapchain->format;
}

uint32_t vr_renderer_get_swapchain_image_count(int eye)
{
    if (!g_vr_renderer.initialized || eye < 0 || eye > 1) {
        return 0;
    }
    
    OpenXRSwapchain* swapchain = (eye == 0) ? g_vr_renderer.leftSwapchain : g_vr_renderer.rightSwapchain;
    if (!swapchain) {
        return 0;
    }
    
    return swapchain->imageCount;
}

int vr_renderer_init_quad_layer(uint32_t width, uint32_t height)
{
    if (!g_vr_renderer.initialized) {
        cerr << "VR renderer not initialized" << endl;
        return 0;
    }
    
    if (g_vr_renderer.quadLayerInitialized) {
        cout << "Quad layer already initialized" << endl;
        return 1;
    }
    
    cout << "Initializing quad layer..." << endl;
    
    if (!createQuadSwapchain(
            g_vr_renderer.xrInstance,
            g_vr_renderer.xrSystemId,
            g_vr_renderer.xrSession,
            width,
            height,
            &g_vr_renderer.quadSwapchain)) {
        cerr << "Failed to create quad swapchain" << endl;
        return 0;
    }
    
    g_vr_renderer.quadLayerInitialized = true;
    return 1;
}

int vr_renderer_render_quad_layer(void)
{
    if (!g_vr_renderer.initialized || !g_vr_renderer.quadLayerInitialized || !g_vr_renderer.frameActive) {
        return 0;
    }
    
    OpenXRSwapchain* swapchain = g_vr_renderer.quadSwapchain;
    
    // Acquire swapchain image
    XrSwapchainImageAcquireInfo acquireInfo{};
    acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    
    uint32_t imageIndex = 0;
    XrResult result = xrAcquireSwapchainImage(swapchain->swapchain, &acquireInfo, &imageIndex);
    
    if (result != XR_SUCCESS) {
        cerr << "Failed to acquire quad swapchain image: " << result << endl;
        return 0;
    }
    
    g_vr_renderer.quadSwapchainIndex = imageIndex;
    
    // Wait for swapchain image
    XrSwapchainImageWaitInfo waitInfo{};
    waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    waitInfo.timeout = XR_INFINITE_DURATION;
    
    result = xrWaitSwapchainImage(swapchain->swapchain, &waitInfo);
    
    if (result != XR_SUCCESS) {
        cerr << "Failed to wait for quad swapchain image: " << result << endl;
        return 0;
    }
    
    g_vr_renderer.quadLayerActive = true;
    return 1;
}

void vr_renderer_set_quad_layer_pose(float position_x, float position_y, float position_z,
                                     float orientation_x, float orientation_y, float orientation_z, float orientation_w)
{
    g_vr_renderer.quadLayer.pose.position = {position_x, position_y, position_z};
    g_vr_renderer.quadLayer.pose.orientation = {orientation_x, orientation_y, orientation_z, orientation_w};
}

void vr_renderer_set_quad_layer_size(float width, float height)
{
    g_vr_renderer.quadLayer.size = {width, height};
}

VkImage vr_renderer_get_quad_swapchain_image(void)
{
    if (!g_vr_renderer.initialized || !g_vr_renderer.quadLayerInitialized) {
        return VK_NULL_HANDLE;
    }
    
    OpenXRSwapchain* swapchain = g_vr_renderer.quadSwapchain;
    if (!swapchain || !swapchain->images) {
        return VK_NULL_HANDLE;
    }
    
    uint32_t imageIndex = g_vr_renderer.quadSwapchainIndex;
    if (imageIndex >= swapchain->imageCount) {
        return VK_NULL_HANDLE;
    }
    
    return swapchain->images[imageIndex];
}

void vr_renderer_get_quad_viewport(uint32_t* width, uint32_t* height)
{
    if (!g_vr_renderer.initialized || !g_vr_renderer.quadLayerInitialized) {
        *width = 0;
        *height = 0;
        return;
    }
    
    *width = g_vr_renderer.quadSwapchain->width;
    *height = g_vr_renderer.quadSwapchain->height;
}

