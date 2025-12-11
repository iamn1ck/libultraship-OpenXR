#if defined(__ANDROID__) || defined(__IOS__)
#include "MobileImpl.h"
#include <SDL2/SDL.h>
#include "public/bridge/consolevariablebridge.h"

#include <imgui_internal.h>

static float cameraYaw;
static float cameraPitch;

static bool isShowingVirtualKeyboard = true;
static bool isUsingTouchscreenControls = true;

void Ship::Mobile::ImGuiProcessEvent(bool wantsTextInput) {
    ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetActiveID());

    if (wantsTextInput) {
        if (!isShowingVirtualKeyboard) {
            state->ClearText();

            isShowingVirtualKeyboard = true;
            SDL_StartTextInput();
        }
    } else {
        if (isShowingVirtualKeyboard) {
            isShowingVirtualKeyboard = false;
            SDL_StopTextInput();
        }
    }
}

#endif
#ifdef __ANDROID__
#include <SDL_gamecontroller.h>
#include <jni.h>

void Ship::Mobile::Init() {
    // None (add here Android initialization steps)
}

void Ship::Mobile::Exit() {
    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
    SDL_Quit();
    exit(0);
}

bool Ship::Mobile::IsUsingTouchscreenControls(){
    return isUsingTouchscreenControls;
}

void Ship::Mobile::EnableTouchArea(){
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject javaObject = (jobject)SDL_AndroidGetActivity();
    jclass javaClass = env->GetObjectClass(javaObject);
    jmethodID enabletoucharea = env->GetMethodID(javaClass, "EnableTouchArea", "()V");
    env->CallVoidMethod(javaObject, enabletoucharea);
}

void Ship::Mobile::DisableTouchArea(){
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject javaObject = (jobject)SDL_AndroidGetActivity();
    jclass javaClass = env->GetObjectClass(javaObject);
    jmethodID disabletoucharea = env->GetMethodID(javaClass, "DisableTouchArea", "()V");
    env->CallVoidMethod(javaObject, disabletoucharea);
}

float Ship::Mobile::GetCameraYaw(){
    return cameraYaw;
}

float Ship::Mobile::GetCameraPitch(){
    return cameraPitch;
}

/**
 * @details Some Android device manufacturer drivers for internal sensors
 * reports the sensor as a generic input in the kernel, causing SDL to wrongly
 * detects them as "gamepads".
 * An example of this are some Xiaomi devices which fingerprint sensor is
 * detected as a gamepad with the identifier "uinput-fpc", but there are more
 * devices and sensors that could also be wrongly detected.
 * Related:
 * https://github.com/godotengine/godot/issues/47656
 */
bool Ship::Mobile::IsInvalidGamepad(const char* gamepad_name)
{
    static const char* device_deny_list[] =
    {
        "uinput-fpc",       // Fingerprint sensor by FPC
        "uinput-fortsense", // Fingerprint sensor by Fortsense
        "uinput-goodix",    // Fingerprint sensor by Goodix
        "uinput-synaptics", // Fingerprint sensor by Synaptics
        "uinput-elan",      // Fingerprint sensor by ElanTech
        "uinput-vfs",       // Fingerprint sensor by Validity
        "uinput-atrus",     // Fingerprint sensor by Atrua
        // ...
        nullptr
    };

    if ( (gamepad_name == nullptr) || (gamepad_name[0] == '\0') )
    {   return true;   }

    for (int i = 0; device_deny_list[i] != nullptr; ++i)
    {
        if (strstr(gamepad_name, device_deny_list[i]))
        {   return true;   }
    }

    return false;
}

static int virtual_joystick_id = -1;
static SDL_Joystick *virtual_joystick = nullptr;

extern "C" void JNICALL Java_com_dishii_soh_SecondaryActivity_attachController(JNIEnv* env, jobject obj) {
    virtual_joystick_id = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 18, 0);
    if (virtual_joystick_id == -1) {
            SDL_Log("Could not create overlay virtual controller");
        return;
    }

    virtual_joystick = SDL_JoystickOpen(virtual_joystick_id);
    if (virtual_joystick == nullptr)
        SDL_Log("Could not create virtual joystick");

    isUsingTouchscreenControls = true;
}

/*extern "C" void JNICALL Java_com_dishii_soh_SecondaryActivity_setCameraState(JNIEnv *env, jobject jobj, jint axis, jfloat value) {
    switch(axis){
        case 0:
            cameraYaw=value;
            break;
        case 1:
            cameraPitch=value;
            break;
    }
}
*/

extern "C" void JNICALL Java_com_dishii_soh_SecondaryActivity_setButton(JNIEnv *env, jobject jobj, jint button, jboolean value) {
    if(button < 0){
        SDL_JoystickSetVirtualAxis(virtual_joystick,-button, value ? SDL_MAX_SINT16 : -SDL_MAX_SINT16); // This should be 0 when false, but I think there's a bug in SDL
    }else{
        SDL_JoystickSetVirtualButton(virtual_joystick, button, value);
    }
}

#include "Context.h"

extern "C" void JNICALL Java_com_dishii_soh_SecondaryActivity_setAxis(JNIEnv *env, jobject jobj, jint axis, jshort value) {
    SDL_JoystickSetVirtualAxis(virtual_joystick, axis, value);
}

extern "C" void JNICALL Java_com_dishii_soh_SecondaryActivity_detachController(JNIEnv *env, jobject jobj) {
    SDL_JoystickClose(virtual_joystick);
    SDL_JoystickDetachVirtual(virtual_joystick_id);
    virtual_joystick = nullptr;
    virtual_joystick_id = -1;
    isUsingTouchscreenControls = false;
}

#endif
