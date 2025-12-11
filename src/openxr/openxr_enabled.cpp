#include <jni.h>
#include <atomic>

// Global atomic flag to control OpenXR initialization
// Default to true (enabled for game), MainActivity will set to false for extractor
static std::atomic<bool> g_openxr_enabled{true};

extern "C" {

JNIEXPORT void JNICALL
Java_com_dishii_soh_MainActivity_setOpenXREnabled(JNIEnv* env, jclass clazz, jboolean enabled) {
    g_openxr_enabled.store(enabled);
}

JNIEXPORT void JNICALL
Java_com_dishii_soh_SecondaryActivity_setOpenXREnabled(JNIEnv* env, jclass clazz, jboolean enabled) {
    g_openxr_enabled.store(enabled);
}

// Function to check if OpenXR should be initialized
bool is_openxr_enabled() {
    return g_openxr_enabled.load();
}

} // extern "C"
