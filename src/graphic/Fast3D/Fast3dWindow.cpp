#include "Fast3dWindow.h"

#include "Context.h"
#include "public/bridge/consolevariablebridge.h"
#include "graphic/Fast3D/gfx_pc.h"
#include "graphic/Fast3D/gfx_sdl.h"
#include "graphic/Fast3D/gfx_dxgi.h"
#include "graphic/Fast3D/gfx_opengl.h"
#include "graphic/Fast3D/gfx_metal.h"
#include "graphic/Fast3D/gfx_direct3d11.h"
#include "graphic/Fast3D/gfx_direct3d12.h"
#include "graphic/Fast3D/gfx_pc.h"

#include "openxr/openxr_manager.h"
#include "openxr/vr_camera.h"
#include "openxr/vr_opengl.h"
#include "openxr/vr_copy.h"
#include "openxr/vr_renderer.h"

#include <GLES3/gl3.h>

#include <fstream>

namespace Fast {
Fast3dWindow::Fast3dWindow(std::shared_ptr<Ship::Gui> gui) : Ship::Window(gui) {
    mWindowManagerApi = nullptr;
    mRenderingApi = nullptr;

#ifdef _WIN32
    AddAvailableWindowBackend(Ship::WindowBackend::FAST3D_DXGI_DX11);
#endif
#ifdef __APPLE__
    if (Metal_IsSupported()) {
        AddAvailableWindowBackend(Ship::WindowBackend::FAST3D_SDL_METAL);
    }
#endif
    AddAvailableWindowBackend(Ship::WindowBackend::FAST3D_SDL_OPENGL);
}

Fast3dWindow::Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>> guiWindows)
    : Fast3dWindow(std::make_shared<Ship::Gui>(guiWindows)) {
}

Fast3dWindow::Fast3dWindow() : Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>>()) {
}

Fast3dWindow::~Fast3dWindow() {
    SPDLOG_DEBUG("destruct fast3dwindow");
    gfx_destroy();
}

void Fast3dWindow::Init() {
    bool gameMode = false;

#if defined(__linux__) && !defined(__ANDROID__)
    std::ifstream osReleaseFile("/etc/os-release");
    if (osReleaseFile.is_open()) {
        std::string line;
        while (std::getline(osReleaseFile, line)) {
            if (line.find("VARIANT_ID") != std::string::npos) {
                if (line.find("steamdeck") != std::string::npos) {
                    gameMode = std::getenv("XDG_CURRENT_DESKTOP") != nullptr &&
                               std::string(std::getenv("XDG_CURRENT_DESKTOP")) == "gamescope";
                }
                break;
            }
        }
    }
#elif defined(__ANDROID__) || defined(__IOS__)
    gameMode = true;
#endif

    bool isFullscreen;
    uint32_t width, height;
    int32_t posX, posY;

    isFullscreen = Ship::Context::GetInstance()->GetConfig()->GetBool("Window.Fullscreen.Enabled", false) || gameMode;
    posX = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.PositionX", 100);
    posY = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.PositionY", 100);

    if (isFullscreen) {
        width = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Fullscreen.Width", gameMode ? 1280 : 1920);
        height = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Fullscreen.Height", gameMode ? 800 : 1080);
    } else {
        width = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Width", 640);
        height = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Height", 480);
    }

    SetForceCursorVisibility(CVarGetInteger("gForceCursorVisibility", 0));

    InitWindowManager();

    gfx_init(mWindowManagerApi, mRenderingApi, Ship::Context::GetInstance()->GetName().c_str(), isFullscreen, width,
             height, posX, posY);
    mWindowManagerApi->set_fullscreen_changed_callback(OnFullscreenChanged);
    mWindowManagerApi->set_keyboard_callbacks(KeyDown, KeyUp, AllKeysUp);
    mWindowManagerApi->set_mouse_callbacks(MouseButtonDown, MouseButtonUp);

    if (openxr_init()) {
        auto session = openxr_get_session();
        auto space = openxr_get_space();

        SPDLOG_WARN("OpenXR initializing");

        if (session == XR_NULL_HANDLE || space == XR_NULL_HANDLE) {
            SPDLOG_WARN("OpenXR initialized but session/space not valid; disabling VR");
            openxr_shutdown();
        } else if (!vr_renderer_init()) {
            SPDLOG_WARN("VR renderer init failed; disabling VR");
            openxr_shutdown();
        } else {
            SPDLOG_INFO("OpenXR session ready!");
        }
    } else {
        SPDLOG_WARN("OpenXR not available; continuing without VR");
    }

    SetTextureFilter((FilteringMode)CVarGetInteger(CVAR_TEXTURE_FILTER, FILTER_THREE_POINT));
}

void Fast3dWindow::SetTargetFps(int32_t fps) {
    gfx_set_target_fps(fps);
}

void Fast3dWindow::SetMaximumFrameLatency(int32_t latency) {
    gfx_set_maximum_frame_latency(latency);
}

void Fast3dWindow::GetPixelDepthPrepare(float x, float y) {
    gfx_get_pixel_depth_prepare(x, y);
}

uint16_t Fast3dWindow::GetPixelDepth(float x, float y) {
    return gfx_get_pixel_depth(x, y);
}

void Fast3dWindow::InitWindowManager() {
    SetWindowBackend(Ship::Context::GetInstance()->GetConfig()->GetWindowBackend());

    switch (GetWindowBackend()) {
#ifdef ENABLE_DX11
        case Ship::WindowBackend::FAST3D_DXGI_DX11:
            mRenderingApi = &gfx_direct3d11_api;
            mWindowManagerApi = &gfx_dxgi_api;
            break;
#endif
#ifdef ENABLE_OPENGL
        case Ship::WindowBackend::FAST3D_SDL_OPENGL:
            mRenderingApi = &gfx_opengl_api;
            mWindowManagerApi = &gfx_sdl;
            break;
#endif
#ifdef __APPLE__
        case Ship::WindowBackend::FAST3D_SDL_METAL:
            mRenderingApi = &gfx_metal_api;
            mWindowManagerApi = &gfx_sdl;
            break;
#endif
        default:
            SPDLOG_ERROR("Could not load the correct rendering backend");
            break;
    }
}

void Fast3dWindow::SetTextureFilter(FilteringMode filteringMode) {
    gfx_get_current_rendering_api()->set_texture_filter(filteringMode);
}

void Fast3dWindow::EnableSRGBMode() {
    gfx_get_current_rendering_api()->enable_srgb_mode();
}

void Fast3dWindow::SetRendererUCode(UcodeHandlers ucode) {
    gfx_set_target_ucode(ucode);
}

void Fast3dWindow::Close() {
    openxr_shutdown();
    mWindowManagerApi->close();
}

void Fast3dWindow::StartFrame() {
    gfx_start_frame();
}

void Fast3dWindow::EndFrame() {
    gfx_end_frame();
}

bool Fast3dWindow::IsFrameReady() {
    return mWindowManagerApi->is_frame_ready();
}

#ifdef OPENXR_ENABLED
static void gfx_setup_vr_matrices_for_eye(int eye) {
    // Fetch VR projection matrix for this eye
    g_rsp.vr_matrices_valid = false;

    if (vr_renderer_get_projection_matrix(eye, (float*)g_rsp.vr_projection_override)) {
        // Fetch VR view matrix (contains IPD offset)
        SPDLOG_WARN("got vr_renderer_get_projection_matrix");

        if (vr_renderer_get_view_matrix(eye, (float*)g_rsp.vr_view_offset)) {
            SPDLOG_WARN("got vr_renderer_get_view_matrix");

            g_rsp.vr_matrices_valid = true;
            g_rsp.vr_current_eye = eye;
        }
    }
}
#endif

bool Fast3dWindow::DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtxReplacements) {
    std::shared_ptr<Window> wnd = Ship::Context::GetInstance()->GetWindow();

    // Skip dropped frames
    if (!wnd->IsFrameReady()) {
        return false;
    }

    auto gui = wnd->GetGui();

#ifdef OPENXR_ENABLED
    // Update OpenXR state and get head pose
    openxr_update();
    // Update VR camera with head tracking
    vr_camera_update();
#endif
    // Check if VR rendering is active
    if (vr_renderer_is_initialized()) {
        SPDLOG_INFO("VR rendering path active");
        if (!vr_renderer_begin_frame()) {
            static int once = 0;
            if (!once) {
                SPDLOG_WARN("VR frame not ready, falling back to normal rendering");
                once = 1;
            }

            SPDLOG_WARN("VR frame not ready, falling back to normal rendering");

            goto normal_rendering;
        }

        static int first_vr_frame = 1;
        if (first_vr_frame) {
            printf("DEBUG: Entering VR rendering path for first time\n");
            first_vr_frame = 0;
        }
        
        // Initialize VR OpenGL and VR copy if needed
        static int vr_gl_initialized = 0;
        static int vr_copy_initialized = 0;
        if (!vr_gl_initialized) {
            if (vr_opengl_init()) {
                vr_gl_initialized = 1;
                printf("VR OpenGL initialized for rendering\n");
                
                // Now initialize VR copy system
                if (vr_copy_init()) {
                    vr_copy_initialized = 1;
                    printf("VR copy system initialized\n");
                } else {
                    fprintf(stderr, "Warning: Failed to initialize VR copy system. VR display may not work.\n");
                }
            } else {
                fprintf(stderr, "Failed to initialize VR OpenGL, falling back to normal rendering\n");
                goto normal_rendering;
            }
        }

        // Save current window dimensions to restore after VR
        auto saved_dimensions = gfx_current_dimensions;

        // Start frame once before rendering both eyes
        gfx_start_frame();

        // Render to each eye
        for (int eye = 0; eye < 2; ++eye) {
            if (!vr_renderer_render_eye(eye)) {
                SPDLOG_WARN("Failed to acquire swapchain for eye {}", eye);
                continue;
            }
            
            uint32_t vr_width = 0, vr_height = 0;
            vr_opengl_get_viewport(eye, &vr_width, &vr_height);
            gfx_current_dimensions.width = vr_width;
            gfx_current_dimensions.height = vr_height;
            gfx_current_dimensions.aspect_ratio = static_cast<float>(vr_width) / (float)vr_height;
            
            // Set up per-eye VR matrices
            gfx_setup_vr_matrices_for_eye(eye);
            
            while (glGetError() != GL_NO_ERROR);
            
            gfx_opengl_set_vr_rendering_mode(true);
            
            static int vr_mode_log = 0;
            if (vr_mode_log < 10) {
                SPDLOG_INFO("VR eye {} enabled VR rendering mode", eye);
                vr_mode_log++;
            }
            
            g_rsp.vr_rendering_active = 1;
            
            // Bind VR framebuffer for this eye
            if (!vr_opengl_begin_eye(eye)) {
                SPDLOG_WARN("Failed to bind framebuffer for eye {}", eye);
                g_rsp.vr_rendering_active = 0;
                gfx_opengl_set_vr_rendering_mode(false);
                continue;
            }
            
            GLenum err_after_bind = glGetError();
            if (err_after_bind != GL_NO_ERROR) {
                SPDLOG_ERROR("GL error after binding VR framebuffer: {}", err_after_bind);
            }
            
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // Check current framebuffer binding before rendering
            GLint current_fbo = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
            GLuint expected_fbo = vr_opengl_get_framebuffer(eye);

            // Render the game to the VR framebuffer
            gfx_run(commands, mtxReplacements);
            
            // Check if framebuffer is still bound after gfx_run
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
            
            // gui->StartDraw(); // Moved to Quad Layer
            // gui->EndDraw();   // Moved to Quad Layer

            
            // Disable VR rendering mode
            gfx_opengl_set_vr_rendering_mode(false);
            int vr_active_before_reset = g_rsp.vr_rendering_active;
            g_rsp.vr_rendering_active = 0;
            
            vr_opengl_end_eye(eye);
        }

        // Restore window dimensions
        gfx_current_dimensions = saved_dimensions;

        // Render Quad Layer (ImGui)
        // Render Quad Layer (ImGui)
        static bool quad_initialized = false;
        if (!quad_initialized) {
            // Initialize quad layer (3840x2160 for better UI resolution)
            if (vr_renderer_init_quad_layer(3840, 2160)) {
                // Set pose (1.5m in front, slightly up)
                vr_renderer_set_quad_layer_pose(0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
                vr_renderer_set_quad_layer_size(1.6f, 0.9f); // 16:9 aspect ratio
                
                // Initialize OpenGL for quad
                vr_opengl_init_quad(3840, 2160);
                
                quad_initialized = true;
                printf("Quad layer initialized for ImGui\n");
            }
        }
        
        if (quad_initialized) {
            // Always bind quad FBO to capture ImGui rendering
            if (vr_opengl_begin_quad()) {
                // Clear to transparent black
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent background
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                
                // Set dimensions for ImGui
                gfx_current_dimensions.width = 3840;
                gfx_current_dimensions.height = 2160;
                
                // Render ImGui
                gui->StartDraw();
                gui->EndDraw();
                
                // Only submit if visible
                if (gui->GetMenuOrMenubarVisible()) {
                    if (vr_renderer_render_quad_layer()) {
                        vr_opengl_end_quad(); // Copies and unbinds
                    } else {
                        vr_opengl_cancel_quad(); // Just unbinds
                    }
                } else {
                    vr_opengl_cancel_quad(); // Just unbinds
                }
            }
        }

        // Render Quad Layer 2 (Hello Triangle)
        static bool quad2_initialized = false;
        if (!quad2_initialized) {
            // Initialize quad layer 2 (512x512 for the triangle)
            if (vr_renderer_init_quad_layer2(3840, 2160)) {
                // Set pose (slightly to the right of the first quad)
                vr_renderer_set_quad_layer2_pose(0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
                vr_renderer_set_quad_layer2_size(0.5f, 0.5f); // 0.5m x 0.5m
                
                // Initialize OpenGL for quad 2
                vr_opengl_init_quad2(3840, 2160);
                
                quad2_initialized = true;
                printf("Quad layer 2 initialized for hello triangle\n");
            }
        }
        
        if (quad2_initialized) {
            if (vr_opengl_begin_quad2()) {
                // Clear to black
                glClearColor(0.0f, 0.0f, 0.0f, 0.5f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                
                // Draw hello triangle
                vr_opengl_draw_hello_triangle();
                
                // Always submit the triangle quad
                if (vr_renderer_render_quad_layer2()) {
                    vr_opengl_end_quad2();
                } else {
                    vr_opengl_cancel_quad2();
                }
            }
        }

        // Restore dimensions again just in case
        gfx_current_dimensions = saved_dimensions;

        vr_renderer_end_frame();

        gfx_end_frame();

        return true;
    }
    
    normal_rendering:

    return true;
}

void Fast3dWindow::HandleEvents() {
    mWindowManagerApi->handle_events();
}

void Fast3dWindow::SetCursorVisibility(bool visible) {
    mWindowManagerApi->set_cursor_visibility(visible);
}

uint32_t Fast3dWindow::GetWidth() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->get_dimensions(&width, &height, &posX, &posY);
    return width;
}

uint32_t Fast3dWindow::GetHeight() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->get_dimensions(&width, &height, &posX, &posY);
    return height;
}

int32_t Fast3dWindow::GetPosX() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->get_dimensions(&width, &height, &posX, &posY);
    return posX;
}

int32_t Fast3dWindow::GetPosY() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->get_dimensions(&width, &height, &posX, &posY);
    return posY;
}

void Fast3dWindow::SetMousePos(Ship::Coords pos) {
    mWindowManagerApi->set_mouse_pos(pos.x, pos.y);
}

Ship::Coords Fast3dWindow::GetMousePos() {
    int32_t x, y;
    mWindowManagerApi->get_mouse_pos(&x, &y);
    return { x, y };
}

Ship::Coords Fast3dWindow::GetMouseDelta() {
    int32_t x, y;
    mWindowManagerApi->get_mouse_delta(&x, &y);
    return { x, y };
}

Ship::CoordsF Fast3dWindow::GetMouseWheel() {
    float x, y;
    mWindowManagerApi->get_mouse_wheel(&x, &y);
    return { x, y };
}

bool Fast3dWindow::GetMouseState(Ship::MouseBtn btn) {
    return mWindowManagerApi->get_mouse_state(static_cast<uint32_t>(btn));
}

void Fast3dWindow::SetMouseCapture(bool capture) {
    mWindowManagerApi->set_mouse_capture(capture);
}

bool Fast3dWindow::IsMouseCaptured() {
    return mWindowManagerApi->is_mouse_captured();
}

uint32_t Fast3dWindow::GetCurrentRefreshRate() {
    uint32_t refreshRate;
    mWindowManagerApi->get_active_window_refresh_rate(&refreshRate);
    return refreshRate;
}

bool Fast3dWindow::SupportsWindowedFullscreen() {
#ifdef __APPLE__
    return false;
#endif

    if (GetWindowBackend() == Ship::WindowBackend::FAST3D_SDL_OPENGL) {
        return true;
    }

    return false;
}

bool Fast3dWindow::CanDisableVerticalSync() {
    return mWindowManagerApi->can_disable_vsync();
}

void Fast3dWindow::SetResolutionMultiplier(float multiplier) {
    gfx_current_dimensions.internal_mul = multiplier;
}

void Fast3dWindow::SetMsaaLevel(uint32_t value) {
    gfx_msaa_level = value;
}

void Fast3dWindow::SetFullscreen(bool isFullscreen) {
    // Save current window position before fullscreening
    SaveWindowToConfig();
    mWindowManagerApi->set_fullscreen(isFullscreen);
}

bool Fast3dWindow::IsFullscreen() {
    return mWindowManagerApi->is_fullscreen();
}

bool Fast3dWindow::IsRunning() {
    return mWindowManagerApi->is_running();
}

const char* Fast3dWindow::GetKeyName(int32_t scancode) {
    return mWindowManagerApi->get_key_name(scancode);
}

bool Fast3dWindow::KeyUp(int32_t scancode) {
    if (scancode ==
        Ship::Context::GetInstance()->GetConfig()->GetInt("Shortcuts.Fullscreen", Ship::KbScancode::LUS_KB_F11)) {
        Ship::Context::GetInstance()->GetWindow()->ToggleFullscreen();
    }

    if (scancode ==
        Ship::Context::GetInstance()->GetConfig()->GetInt("Shortcuts.MouseCapture", Ship::KbScancode::LUS_KB_F2)) {
        bool captureState = Ship::Context::GetInstance()->GetWindow()->IsMouseCaptured();
        Ship::Context::GetInstance()->GetWindow()->SetMouseCapture(!captureState);
    }

    Ship::Context::GetInstance()->GetWindow()->SetLastScancode(-1);
    return Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(
        Ship::KbEventType::LUS_KB_EVENT_KEY_UP, static_cast<Ship::KbScancode>(scancode));
}

bool Fast3dWindow::KeyDown(int32_t scancode) {
    bool isProcessed = Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(
        Ship::KbEventType::LUS_KB_EVENT_KEY_DOWN, static_cast<Ship::KbScancode>(scancode));
    Ship::Context::GetInstance()->GetWindow()->SetLastScancode(scancode);

    return isProcessed;
}

void Fast3dWindow::AllKeysUp() {
    Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(Ship::KbEventType::LUS_KB_EVENT_ALL_KEYS_UP,
                                                                         Ship::KbScancode::LUS_KB_UNKNOWN);
}

bool Fast3dWindow::MouseButtonUp(int button) {
    return Ship::Context::GetInstance()->GetControlDeck()->ProcessMouseButtonEvent(false,
                                                                                   static_cast<Ship::MouseBtn>(button));
}

bool Fast3dWindow::MouseButtonDown(int button) {
    bool isProcessed = Ship::Context::GetInstance()->GetControlDeck()->ProcessMouseButtonEvent(
        true, static_cast<Ship::MouseBtn>(button));
    return isProcessed;
}

void Fast3dWindow::OnFullscreenChanged(bool isNowFullscreen) {
    std::shared_ptr<Window> wnd = Ship::Context::GetInstance()->GetWindow();

    if (isNowFullscreen) {
        auto menuVisible = wnd->GetGui()->GetMenuOrMenubarVisible();
        wnd->SetMouseCapture(!(menuVisible || wnd->ShouldForceCursorVisibility()));
    } else {
        wnd->SetMouseCapture(false);
    }

    // Re-save fullscreen enabled after
    Ship::Context::GetInstance()->GetConfig()->SetBool("Window.Fullscreen.Enabled", isNowFullscreen);
}
} // namespace Fast
