#include <chorus/app/app_controller.hpp>
#include <nlohmann/json.hpp>

#include <string>

#if defined(__has_include)
#if __has_include(<jni.h>)
#include <jni.h>
#else
// Fallback definitions for host IDE/static analyzers
using jlong = long long;
using jint = int;
using jboolean = unsigned char;
using jfloat = float;
using jobject = void*;
using jstring = void*;
inline constexpr jboolean JNI_FALSE = 0;
inline constexpr jboolean JNI_TRUE = 1;
#define JNIEXPORT
#define JNICALL

struct JNIEnv {
    static const char* GetStringUTFChars(jstring /*str*/, jboolean* /*isCopy*/) { return nullptr; }
    static void ReleaseStringUTFChars(jstring /*str*/, const char* /*chars*/) {}
    static jstring NewStringUTF(const char* /*bytes*/) { return nullptr; }
};
#endif

#if __has_include(<android/log.h>)
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ChorusJNI", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ChorusJNI", __VA_ARGS__)
#else
template <typename... Args>
inline void LOGI(Args&&...) {}
template <typename... Args>
inline void LOGE(Args&&...) {}
#endif
#else
#include <android/log.h>
#include <jni.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ChorusJNI", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ChorusJNI", __VA_ARGS__)
#endif

namespace {

chorus::AppController* get_controller(jlong handle) {
    return reinterpret_cast<chorus::AppController*>(handle);  // NOLINT(performance-no-int-to-ptr)
}

std::string jstring_to_utf8(JNIEnv* env, jstring jstr) {
    if (jstr == nullptr) {
        return "";
    }
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    if (chars == nullptr) {
        return "";
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeInit(JNIEnv* /*env*/, jobject /*thiz*/) {
    LOGI("Initializing native Chorus AppController");
    auto* controller = new chorus::AppController();
    return reinterpret_cast<jlong>(controller);  // NOLINT(performance-no-int-to-ptr)
}

JNIEXPORT void JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeCleanup(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    LOGI("Cleaning up native Chorus AppController");
    auto* controller = get_controller(handle);
    delete controller;
}

JNIEXPORT jboolean JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeStartHost(
    JNIEnv* env, jobject /*thiz*/, jlong handle, jstring pin, jlong target_latency_ms, jboolean use_test_tone) {
    auto* controller = get_controller(handle);
    if (controller == nullptr) {
        return JNI_FALSE;
    }
    std::string pin_str = jstring_to_utf8(env, pin);
    bool ok = controller->start_host(pin_str, static_cast<uint64_t>(target_latency_ms), use_test_tone == JNI_TRUE);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeStopHost(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    auto* controller = get_controller(handle);
    if (controller != nullptr) {
        controller->stop_host();
    }
}

JNIEXPORT jboolean JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeJoinHost(
    JNIEnv* env, jobject /*thiz*/, jlong handle, jstring host_ip, jint port, jstring pin) {
    auto* controller = get_controller(handle);
    if (controller == nullptr) {
        return JNI_FALSE;
    }
    std::string ip_str = jstring_to_utf8(env, host_ip);
    std::string pin_str = jstring_to_utf8(env, pin);
    bool ok = controller->join_host(ip_str, static_cast<uint16_t>(port), pin_str);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeLeaveHost(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    auto* controller = get_controller(handle);
    if (controller != nullptr) {
        controller->leave_host();
    }
}

JNIEXPORT void JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeUpdate(JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    auto* controller = get_controller(handle);
    if (controller != nullptr) {
        controller->update();
    }
}

JNIEXPORT void JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeSetVolume(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle, jfloat volume) {
    auto* controller = get_controller(handle);
    if (controller != nullptr) {
        controller->set_volume(volume);
    }
}

JNIEXPORT void JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeSetMute(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle, jboolean mute) {
    auto* controller = get_controller(handle);
    if (controller != nullptr) {
        controller->set_mute(mute == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeSetOffsetMs(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle, jint offset_ms) {
    auto* controller = get_controller(handle);
    if (controller != nullptr) {
        controller->set_offset_ms(offset_ms);
    }
}

JNIEXPORT jstring JNICALL
Java_dev_chorus_app_service_NativeChorusBridge_nativeGetSnapshotJson(
    JNIEnv* env, jobject /*thiz*/, jlong handle) {
    auto* controller = get_controller(handle);
    if (controller == nullptr) {
        return env->NewStringUTF("{}");
    }

    chorus::AppSnapshot snap = controller->snapshot();

    std::string role_str = "idle";
    if (snap.role == chorus::AppRole::Hosting) {
        role_str = "hosting";
    } else if (snap.role == chorus::AppRole::Client) {
        role_str = "client";
    }

    nlohmann::json j;
    j["role"] = role_str;
    j["is_active"] = snap.is_active;
    j["session_pin"] = snap.session_pin;
    j["rejection_reason"] = snap.rejection_reason;
    j["volume"] = snap.volume;
    j["is_muted"] = snap.is_muted;
    j["offset_ms"] = snap.offset_ms;

    // Client stats
    j["stats"] = {
        {"sync_error_us", snap.client_stats.sync_error_us},
        {"skew_ppm", snap.client_stats.skew_ppm},
        {"underruns", snap.client_stats.underruns},
        {"late_frames", snap.client_stats.late},
        {"loss_pct", snap.client_stats.loss_pct},
        {"buffer_ms", snap.client_stats.buffer_ms}
    };

    // Connected clients (if hosting)
    auto clients_arr = nlohmann::json::array();
    for (const auto& c : snap.connected_clients) {
        clients_arr.push_back({
            {"client_id", c.id},
            {"name", c.name},
            {"endpoint", c.address + ":" + std::to_string(c.control_port)},
            {"volume", c.volume},
            {"is_muted", c.is_muted},
            {"offset_ms", c.offset_ms},
            {"sync_error_us", c.last_stats.sync_error_us}
        });
    }
    j["connected_clients"] = clients_arr;

    // Discovered hosts
    auto hosts_arr = nlohmann::json::array();
    for (const auto& h : snap.discovered_hosts) {
        hosts_arr.push_back({
            {"name", h.name},
            {"ip", h.address},
            {"control_port", h.tcp_port},
            {"audio_port", h.udp_port}
        });
    }
    j["discovered_hosts"] = hosts_arr;

    std::string serialized = j.dump();
    return env->NewStringUTF(serialized.c_str());
}

}  // extern "C"
