#include "openxr_input.h"
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <spdlog/spdlog.h>
#include <cmath>

struct OpenXRInputState {
    XrActionSet actionSet;
    
    // Actions
    XrAction actionA;
    XrAction actionB;
    XrAction actionX;
    XrAction actionY;
    XrAction actionMenu;
    XrAction actionThumbstickLeft;
    XrAction actionThumbstickRight;
    XrAction actionThumbstickClickLeft;
    XrAction actionThumbstickClickRight;
    XrAction actionTriggerLeft;
    XrAction actionTriggerRight;
    XrAction actionGripLeft;
    XrAction actionGripRight;

    // Current state
    bool buttonA;
    bool buttonB;
    bool buttonX;
    bool buttonY;
    bool buttonMenu;
    bool buttonThumbstickLeft;
    bool buttonThumbstickRight;
    float triggerLeft;
    float triggerRight;
    float gripLeft;
    float gripRight;
    XrVector2f thumbstickLeft;
    XrVector2f thumbstickRight;
};

static OpenXRInputState g_input_state = { XR_NULL_HANDLE };
static bool g_input_initialized = false;

static XrAction createAction(XrActionSet actionSet, XrActionType type, const char* name, const char* localizedName) {
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    strcpy(actionInfo.actionName, name);
    strcpy(actionInfo.localizedActionName, localizedName);
    actionInfo.actionType = type;
    
    XrAction action;
    if (xrCreateAction(actionSet, &actionInfo, &action) != XR_SUCCESS) {
        SPDLOG_ERROR("Failed to create OpenXR action: {}", name);
        return XR_NULL_HANDLE;
    }
    return action;
}

int openxr_input_init(XrInstance instance) {
    if (g_input_initialized) return 1;

    SPDLOG_INFO("Initializing OpenXR input...");

    // Create action set
    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy(actionSetInfo.actionSetName, "gameplay");
    strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
    actionSetInfo.priority = 0;
    
    if (xrCreateActionSet(instance, &actionSetInfo, &g_input_state.actionSet) != XR_SUCCESS) {
        SPDLOG_ERROR("Failed to create OpenXR action set");
        return 0;
    }

    // Create actions
    g_input_state.actionA = createAction(g_input_state.actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "a_button", "A Button");
    g_input_state.actionB = createAction(g_input_state.actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "b_button", "B Button");
    g_input_state.actionX = createAction(g_input_state.actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "x_button", "X Button");
    g_input_state.actionY = createAction(g_input_state.actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "y_button", "Y Button");
    g_input_state.actionMenu = createAction(g_input_state.actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu_button", "Menu Button");
    
    g_input_state.actionThumbstickLeft = createAction(g_input_state.actionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_left", "Left Thumbstick");
    g_input_state.actionThumbstickRight = createAction(g_input_state.actionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick_right", "Right Thumbstick");
    
    g_input_state.actionThumbstickClickLeft = createAction(g_input_state.actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_click_left", "Left Thumbstick Click");
    g_input_state.actionThumbstickClickRight = createAction(g_input_state.actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_click_right", "Right Thumbstick Click");

    g_input_state.actionTriggerLeft = createAction(g_input_state.actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trigger_left", "Left Trigger");
    g_input_state.actionTriggerRight = createAction(g_input_state.actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trigger_right", "Right Trigger");
    
    g_input_state.actionGripLeft = createAction(g_input_state.actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_left", "Left Grip");
    g_input_state.actionGripRight = createAction(g_input_state.actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "grip_right", "Right Grip");

    // Suggest bindings for Oculus Touch
    XrPath oculusTouchPath;
    xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", &oculusTouchPath);

    std::vector<XrActionSuggestedBinding> bindings;
    auto addBinding = [&](XrAction action, const char* pathStr) {
        XrPath path;
        xrStringToPath(instance, pathStr, &path);
        bindings.push_back({action, path});
    };

    addBinding(g_input_state.actionA, "/user/hand/right/input/a/click");
    addBinding(g_input_state.actionB, "/user/hand/right/input/b/click");
    addBinding(g_input_state.actionX, "/user/hand/left/input/x/click");
    addBinding(g_input_state.actionY, "/user/hand/left/input/y/click");
    addBinding(g_input_state.actionMenu, "/user/hand/left/input/menu/click");
    
    addBinding(g_input_state.actionThumbstickLeft, "/user/hand/left/input/thumbstick");
    addBinding(g_input_state.actionThumbstickRight, "/user/hand/right/input/thumbstick");
    
    addBinding(g_input_state.actionThumbstickClickLeft, "/user/hand/left/input/thumbstick/click");
    addBinding(g_input_state.actionThumbstickClickRight, "/user/hand/right/input/thumbstick/click");

    addBinding(g_input_state.actionTriggerLeft, "/user/hand/left/input/trigger/value");
    addBinding(g_input_state.actionTriggerRight, "/user/hand/right/input/trigger/value");
    
    addBinding(g_input_state.actionGripLeft, "/user/hand/left/input/squeeze/value");
    addBinding(g_input_state.actionGripRight, "/user/hand/right/input/squeeze/value");

    XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggestedBindings.interactionProfile = oculusTouchPath;
    suggestedBindings.suggestedBindings = bindings.data();
    suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();

    if (xrSuggestInteractionProfileBindings(instance, &suggestedBindings) != XR_SUCCESS) {
        SPDLOG_ERROR("Failed to suggest Oculus Touch bindings");
    }

    g_input_initialized = true;
    return 1;
}

int openxr_input_attach_session(XrSession session) {
    if (!g_input_initialized) return 0;

    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &g_input_state.actionSet;

    if (xrAttachSessionActionSets(session, &attachInfo) != XR_SUCCESS) {
        SPDLOG_ERROR("Failed to attach action sets to session");
        return 0;
    }
    return 1;
}

int openxr_input_sync(XrSession session) {
    if (!g_input_initialized) return 0;

    XrActiveActionSet activeActionSet{g_input_state.actionSet, XR_NULL_PATH};
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;

    if (xrSyncActions(session, &syncInfo) != XR_SUCCESS) {
        return 0;
    }

    auto getBool = [&](XrAction action) -> bool {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (xrGetActionStateBoolean(session, &getInfo, &state) == XR_SUCCESS) {
            return state.currentState && state.isActive;
        }
        return false;
    };

    auto getFloat = [&](XrAction action) -> float {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        if (xrGetActionStateFloat(session, &getInfo, &state) == XR_SUCCESS) {
            return state.isActive ? state.currentState : 0.0f;
        }
        return 0.0f;
    };

    auto getVec2 = [&](XrAction action) -> XrVector2f {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        if (xrGetActionStateVector2f(session, &getInfo, &state) == XR_SUCCESS) {
            return state.isActive ? state.currentState : XrVector2f{0.0f, 0.0f};
        }
        return {0.0f, 0.0f};
    };

    g_input_state.buttonA = getBool(g_input_state.actionA);
    g_input_state.buttonB = getBool(g_input_state.actionB);
    g_input_state.buttonX = getBool(g_input_state.actionX);
    g_input_state.buttonY = getBool(g_input_state.actionY);
    g_input_state.buttonMenu = getBool(g_input_state.actionMenu);
    g_input_state.buttonThumbstickLeft = getBool(g_input_state.actionThumbstickClickLeft);
    g_input_state.buttonThumbstickRight = getBool(g_input_state.actionThumbstickClickRight);
    
    g_input_state.triggerLeft = getFloat(g_input_state.actionTriggerLeft);
    g_input_state.triggerRight = getFloat(g_input_state.actionTriggerRight);
    g_input_state.gripLeft = getFloat(g_input_state.actionGripLeft);
    g_input_state.gripRight = getFloat(g_input_state.actionGripRight);
    
    g_input_state.thumbstickLeft = getVec2(g_input_state.actionThumbstickLeft);
    g_input_state.thumbstickRight = getVec2(g_input_state.actionThumbstickRight);

    return 1;
}

int openxr_get_button(int sdl_button) {
    if (!g_input_initialized) return 0;

    switch (sdl_button) {
        case SDL_CONTROLLER_BUTTON_A: return g_input_state.buttonA;
        case SDL_CONTROLLER_BUTTON_B: return g_input_state.buttonB;
        case SDL_CONTROLLER_BUTTON_X: return g_input_state.buttonX;
        case SDL_CONTROLLER_BUTTON_Y: return g_input_state.buttonY;
        case SDL_CONTROLLER_BUTTON_START: return g_input_state.buttonMenu;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return g_input_state.buttonThumbstickLeft;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return g_input_state.buttonThumbstickRight;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return g_input_state.gripLeft > 0.5f;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return g_input_state.gripRight > 0.5f;
        default: return 0;
    }
}

int16_t openxr_get_axis(int sdl_axis) {
    if (!g_input_initialized) return 0;

    const float DEADZONE = 0.1f;
    auto applyDeadzone = [&](float val) -> int16_t {
        if (std::abs(val) < DEADZONE) return 0;
        return (int16_t)(val * 32767.0f);
    };

    switch (sdl_axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: return applyDeadzone(g_input_state.thumbstickLeft.x);
        case SDL_CONTROLLER_AXIS_LEFTY: return applyDeadzone(-g_input_state.thumbstickLeft.y); // Y is usually inverted in SDL
        case SDL_CONTROLLER_AXIS_RIGHTX: return applyDeadzone(g_input_state.thumbstickRight.x);
        case SDL_CONTROLLER_AXIS_RIGHTY: return applyDeadzone(-g_input_state.thumbstickRight.y);
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return (int16_t)(g_input_state.triggerLeft * 32767.0f);
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return (int16_t)(g_input_state.triggerRight * 32767.0f);
        default: return 0;
    }
}

void openxr_input_shutdown(void) {
    if (!g_input_initialized) return;
    
    // Actions are destroyed with the instance, so we just reset state
    g_input_initialized = false;
    memset(&g_input_state, 0, sizeof(g_input_state));
}
