#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Check if OpenXR should be initialized
// Returns true if SecondaryActivity has enabled OpenXR, false otherwise
bool is_openxr_enabled();

#ifdef __cplusplus
}
#endif
