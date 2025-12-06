#ifndef OPENXR_INPUT_H
#define OPENXR_INPUT_H

#ifdef __cplusplus
#include <openxr/openxr.h>
#include <SDL2/SDL.h>

extern "C" {
#endif

// Initialize OpenXR input (create actions, suggest bindings)
// Must be called after XrInstance is created but before XrSession is created (ideally)
// Actually actions need instance. Action sets need to be attached to session.
int openxr_input_init(XrInstance instance);

// Attach action sets to session
// Must be called before xrBeginSession
int openxr_input_attach_session(XrSession session);

// Sync input actions (call once per frame)
int openxr_input_sync(XrSession session);

// Get button state (maps to SDL_GameControllerButton)
int openxr_get_button(int sdl_button);

// Get axis state (maps to SDL_GameControllerAxis)
int16_t openxr_get_axis(int sdl_axis);

void openxr_input_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // OPENXR_INPUT_H
