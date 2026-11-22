/*
 * Earth And Legend (PS Vita) - FalsoJNI callback implementation.
 *
 * The ShiVa3D engine calls back into the original Android Java code for
 * audio, billing, locale and build information. We replace all of that with
 * native stubs so that the engine can run without a Java VM.
 */

#include <stdarg.h>
#include <string.h>

#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>

#include "utils/logger.h"
#include "utils/trace.h"
#include "java.h"

#define ENV ((JNIEnv *) &jni)

static const char * method_name(jmethodID id);

/*
 * Method IDs. The numeric values are arbitrary but must be unique.
 */
enum {
    M_ON_ENABLE_ACCELEROMETER_UPDATES = 1,
    M_ON_ENABLE_HEADING_UPDATES,
    M_ON_ENABLE_LOCATION_UPDATES,
    M_ON_INIT_SOUND,
    M_ON_LOAD_MUSIC,
    M_ON_LOAD_SOUND,
    M_ON_PLAY_MUSIC,
    M_ON_PLAY_SOUND,
    M_ON_OPEN_URL,
    M_ON_PAUSE_MUSIC,
    M_ON_PAUSE_SOUND,
    M_ON_RESUME_MUSIC,
    M_ON_RESUME_SOUND,
    M_ON_SET_MUSIC_VOLUME,
    M_ON_SET_SOUND_LOOPING,
    M_ON_SET_SOUND_PITCH,
    M_ON_SET_SOUND_VOLUME,
    M_ON_SHUTDOWN_SOUND,
    M_ON_STOP_MUSIC,
    M_ON_STOP_SOUND,
    M_ON_UNLOAD_MUSIC,
    M_ON_UNLOAD_SOUND,
    M_FETCH_PRODUCTS,
    M_IN_APP_RESTORE,
    M_PURCHASE_PRODUCT,
    M_GET_DEFAULT,
    M_TO_STRING,
    M_GET_MAIN_ACTIVITY,
    M_GET_MAIN_LAYOUT,
    M_GET_MAIN_VIEW,
    M_GET_RESOURCE_ID_BY_NAME,
    M_GET_WINDOW_MANAGER,
    M_GET_DEFAULT_DISPLAY,
    M_GET_ROTATION,
    M_GET_WIDTH,
    M_GET_HEIGHT,
    M_GET_NAME,
    M_GET_LANGUAGE,
};

/*
 * Field IDs.
 */
enum {
    F_SDK_INT = 1,
    F_RELEASE,
    F_MODEL,
    F_DESCRIPTOR,
};

static jstring locale_string = NULL;

/*
 * Boolean callbacks.
 */

static jboolean m_on_enable_accelerometer_updates(jmethodID id, va_list args) {
    (void) va_arg(args, int); // jboolean
    return JNI_FALSE;
}

static jboolean m_on_enable_heading_updates(jmethodID id, va_list args) {
    (void) va_arg(args, int); // jboolean
    return JNI_FALSE;
}

static jboolean m_on_enable_location_updates(jmethodID id, va_list args) {
    (void) va_arg(args, int); // jboolean
    return JNI_FALSE;
}

static jboolean m_on_init_sound(jmethodID id, va_list args) {
    (void) args;
    l_success("Java: onInitSound() -> false");
    eal_trace("java: onInitSound -> 0");
    return JNI_FALSE;
}

/*
 * Int callbacks.
 */

static jint m_on_load_sound(jmethodID id, va_list args) {
    jstring name = va_arg(args, jstring);
    const char *text = name ? jni->GetStringUTFChars(ENV, name, NULL) : NULL;
    jint handle = 0;

    eal_trace("java: onLoadSound('%s') -> %d", text ? text : "?", handle);

    if (text)
        jni->ReleaseStringUTFChars(ENV, name, (char *) text);

    return handle;
}

static jint m_on_load_music(jmethodID id, va_list args) {
    jstring name = va_arg(args, jstring);
    const char *text = name ? jni->GetStringUTFChars(ENV, name, NULL) : NULL;
    jint handle = 0;

    eal_trace("java: onLoadMusic('%s') -> %d", text ? text : "?", handle);

    if (text)
        jni->ReleaseStringUTFChars(ENV, name, (char *) text);

    return handle;
}

static jint m_on_play_sound(jmethodID id, va_list args) {
    jint handle = va_arg(args, jint);
    double volume = va_arg(args, double); // float volume
    int looping = va_arg(args, int);      // jboolean looping
    double pitch = va_arg(args, double);  // float pitch

    jint stream = -1;

    eal_trace("java: onPlaySound(handle=%d, vol=%.2f, loop=%d, pitch=%.2f) -> %d",
              handle, volume, looping, pitch, stream);

    return stream;
}

static jint m_on_play_music(jmethodID id, va_list args) {
    jint handle = va_arg(args, jint);
    double volume = va_arg(args, double); // float volume
    int looping = va_arg(args, int);      // jboolean looping
    double pitch = va_arg(args, double);  // float pitch

    jint result = -1;

    eal_trace("java: onPlayMusic(handle=%d, vol=%.2f, loop=%d, pitch=%.2f) -> %d",
              handle, volume, looping, pitch, result);

    return result;
}

static jint m_get_resource_id_by_name(jmethodID id, va_list args) {
    (void) va_arg(args, jstring);
    (void) va_arg(args, jstring);
    // The third overload has one more string argument; va_arg on a missing
    // argument would be undefined behaviour, so we cannot consume it here.
    return 0;
}

static jint m_get_rotation(jmethodID id, va_list args) {
    (void) args;
    return 0;
}

static jint m_get_width(jmethodID id, va_list args) {
    (void) args;
    return 960;
}

static jint m_get_height(jmethodID id, va_list args) {
    (void) args;
    return 544;
}

/*
 * Object callbacks.
 */

static jobject m_get_default(jmethodID id, va_list args) {
    (void) args;
    return locale_string;
}

static jobject m_to_string(jmethodID id, va_list args) {
    (void) args;
    return locale_string;
}

static jobject m_get_main_object(jmethodID id, va_list args) {
    (void) args;
    return NULL;
}

static jobject m_get_name(jmethodID id, va_list args) {
    (void) args;
    return locale_string;
}

/*
 * Void callbacks.
 */

static void m_noop(jmethodID id, va_list args) {
    (void) args;
    eal_trace("java: %s()", method_name(id));
}

static void m_on_open_url(jmethodID id, va_list args) {
    jstring url = va_arg(args, jstring);
    jstring mime = va_arg(args, jstring);
    (void) url;
    (void) mime;
}

static void m_on_purchase_product(jmethodID id, va_list args) {
    (void) va_arg(args, jstring);
}

static void m_on_set_sound_looping(jmethodID id, va_list args) {
    jint handle = va_arg(args, jint);
    int looping = va_arg(args, int); // jboolean
    eal_trace("java: %s(%d, %d)", method_name(id), handle, looping);
}

static void m_on_set_float(jmethodID id, va_list args) {
    jint handle = va_arg(args, jint);
    double value = va_arg(args, double); // float
    eal_trace("java: %s(%d, %.2f)", method_name(id), handle, value);
}

static void m_on_set_int(jmethodID id, va_list args) {
    jint handle = va_arg(args, jint);
    eal_trace("java: %s(%d)", method_name(id), handle);
}

static void m_on_unload_music(jmethodID id, va_list args) {
    jint handle = va_arg(args, jint);
    eal_trace("java: onUnloadMusic(%d)", handle);
}

/*
 * Tables.
 */

NameToMethodID nameToMethodId[] = {
    { M_ON_ENABLE_ACCELEROMETER_UPDATES, "onEnableAccelerometerUpdates", METHOD_TYPE_BOOLEAN },
    { M_ON_ENABLE_HEADING_UPDATES,       "onEnableHeadingUpdates",       METHOD_TYPE_BOOLEAN },
    { M_ON_ENABLE_LOCATION_UPDATES,      "onEnableLocationUpdates",      METHOD_TYPE_BOOLEAN },
    { M_ON_INIT_SOUND,                   "onInitSound",                  METHOD_TYPE_BOOLEAN },
    { M_ON_LOAD_MUSIC,                   "onLoadMusic",                  METHOD_TYPE_INT },
    { M_ON_LOAD_SOUND,                   "onLoadSound",                  METHOD_TYPE_INT },
    { M_ON_PLAY_MUSIC,                   "onPlayMusic",                  METHOD_TYPE_INT },
    { M_ON_PLAY_SOUND,                   "onPlaySound",                  METHOD_TYPE_INT },
    { M_ON_OPEN_URL,                     "onOpenURL",                    METHOD_TYPE_VOID },
    { M_ON_PAUSE_MUSIC,                  "onPauseMusic",                 METHOD_TYPE_VOID },
    { M_ON_PAUSE_SOUND,                  "onPauseSound",                 METHOD_TYPE_VOID },
    { M_ON_RESUME_MUSIC,                 "onResumeMusic",                METHOD_TYPE_VOID },
    { M_ON_RESUME_SOUND,                 "onResumeSound",                METHOD_TYPE_VOID },
    { M_ON_SET_MUSIC_VOLUME,             "onSetMusicVolume",             METHOD_TYPE_VOID },
    { M_ON_SET_SOUND_LOOPING,            "onSetSoundLooping",            METHOD_TYPE_VOID },
    { M_ON_SET_SOUND_PITCH,              "onSetSoundPitch",              METHOD_TYPE_VOID },
    { M_ON_SET_SOUND_VOLUME,             "onSetSoundVolume",             METHOD_TYPE_VOID },
    { M_ON_SHUTDOWN_SOUND,               "onShutdownSound",              METHOD_TYPE_VOID },
    { M_ON_STOP_MUSIC,                   "onStopMusic",                  METHOD_TYPE_VOID },
    { M_ON_STOP_SOUND,                   "onStopSound",                  METHOD_TYPE_VOID },
    { M_ON_UNLOAD_MUSIC,                 "onUnloadMusic",                METHOD_TYPE_VOID },
    { M_ON_UNLOAD_SOUND,                 "onUnloadSound",                METHOD_TYPE_VOID },
    { M_FETCH_PRODUCTS,                  "fetchProducts",                METHOD_TYPE_VOID },
    { M_IN_APP_RESTORE,                  "inAppRestore",                 METHOD_TYPE_VOID },
    { M_PURCHASE_PRODUCT,                "purchaseProduct",              METHOD_TYPE_VOID },
    { M_GET_DEFAULT,                     "getDefault",                   METHOD_TYPE_OBJECT },
    { M_TO_STRING,                       "toString",                     METHOD_TYPE_OBJECT },
    { M_GET_MAIN_ACTIVITY,               "getMainActivity",              METHOD_TYPE_OBJECT },
    { M_GET_MAIN_LAYOUT,                 "getMainLayout",                METHOD_TYPE_OBJECT },
    { M_GET_MAIN_VIEW,                   "getMainView",                  METHOD_TYPE_OBJECT },
    { M_GET_RESOURCE_ID_BY_NAME,         "getResourceIdByName",          METHOD_TYPE_INT },
    { M_GET_WINDOW_MANAGER,              "getWindowManager",             METHOD_TYPE_OBJECT },
    { M_GET_DEFAULT_DISPLAY,             "getDefaultDisplay",            METHOD_TYPE_OBJECT },
    { M_GET_ROTATION,                    "getRotation",                  METHOD_TYPE_INT },
    { M_GET_WIDTH,                       "getWidth",                     METHOD_TYPE_INT },
    { M_GET_HEIGHT,                      "getHeight",                    METHOD_TYPE_INT },
    { M_GET_NAME,                        "getName",                      METHOD_TYPE_OBJECT },
    { M_GET_LANGUAGE,                    "getLanguage",                  METHOD_TYPE_OBJECT },
};

static const char * method_name(jmethodID id) {
    for (unsigned i = 0;
         i < sizeof(nameToMethodId) / sizeof(nameToMethodId[0]); i++) {
        if ((int) id == nameToMethodId[i].id)
            return nameToMethodId[i].name;
    }
    return "?";
}

MethodsBoolean methodsBoolean[] = {
    { M_ON_ENABLE_ACCELEROMETER_UPDATES, m_on_enable_accelerometer_updates },
    { M_ON_ENABLE_HEADING_UPDATES,       m_on_enable_heading_updates },
    { M_ON_ENABLE_LOCATION_UPDATES,      m_on_enable_location_updates },
    { M_ON_INIT_SOUND,                   m_on_init_sound },
};

MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {
    { M_ON_LOAD_MUSIC,           m_on_load_music },
    { M_ON_LOAD_SOUND,           m_on_load_sound },
    { M_ON_PLAY_MUSIC,           m_on_play_music },
    { M_ON_PLAY_SOUND,           m_on_play_sound },
    { M_GET_RESOURCE_ID_BY_NAME, m_get_resource_id_by_name },
    { M_GET_ROTATION,            m_get_rotation },
    { M_GET_WIDTH,               m_get_width },
    { M_GET_HEIGHT,              m_get_height },
};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {
    { M_GET_DEFAULT,         m_get_default },
    { M_TO_STRING,           m_to_string },
    { M_GET_MAIN_ACTIVITY,   m_get_main_object },
    { M_GET_MAIN_LAYOUT,     m_get_main_object },
    { M_GET_MAIN_VIEW,       m_get_main_object },
    { M_GET_WINDOW_MANAGER,  m_get_main_object },
    { M_GET_DEFAULT_DISPLAY, m_get_main_object },
    { M_GET_NAME,            m_get_name },
    { M_GET_LANGUAGE,        m_get_name },
};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {
    { M_ON_OPEN_URL,          m_on_open_url },
    { M_ON_PAUSE_MUSIC,       m_noop },
    { M_ON_PAUSE_SOUND,       m_noop },
    { M_ON_RESUME_MUSIC,      m_noop },
    { M_ON_RESUME_SOUND,      m_noop },
    { M_ON_SET_MUSIC_VOLUME,  m_on_set_float },
    { M_ON_SET_SOUND_LOOPING, m_on_set_sound_looping },
    { M_ON_SET_SOUND_PITCH,   m_on_set_float },
    { M_ON_SET_SOUND_VOLUME,  m_on_set_float },
    { M_ON_SHUTDOWN_SOUND,    m_noop },
    { M_ON_STOP_MUSIC,        m_on_set_int },
    { M_ON_STOP_SOUND,        m_on_set_int },
    { M_ON_UNLOAD_MUSIC,      m_on_unload_music },
    { M_ON_UNLOAD_SOUND,      m_on_set_int },
    { M_FETCH_PRODUCTS,       m_noop },
    { M_IN_APP_RESTORE,       m_noop },
    { M_PURCHASE_PRODUCT,     m_on_purchase_product },
};

/*
 * Fields.
 */

NameToFieldID nameToFieldId[] = {
    { F_SDK_INT,    "SDK_INT",    FIELD_TYPE_INT },
    { F_RELEASE,    "RELEASE",    FIELD_TYPE_OBJECT },
    { F_MODEL,      "MODEL",      FIELD_TYPE_OBJECT },
    { F_DESCRIPTOR, "descriptor", FIELD_TYPE_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
    { F_SDK_INT,    19 }, // Android 4.4 / KitKat
    { F_DESCRIPTOR, 0 },
};
FieldsObject fieldsObject[] = {
    { F_RELEASE, NULL },
    { F_MODEL,   NULL },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES

/*
 * Runtime initialisation.
 */

void java_init_values(void) {
    locale_string = jni->NewStringUTF(ENV, "en_US");

    setObjectFieldValueById((jfieldID) (uintptr_t) F_RELEASE,
                            jni->NewStringUTF(ENV, "4.4.2"));
    setObjectFieldValueById((jfieldID) (uintptr_t) F_MODEL,
                            jni->NewStringUTF(ENV, "PlayStation Vita"));

    l_success("Java values initialized.");
}

void java_set_pack_fd(int fd) {
    setIntFieldValueById((jfieldID) (uintptr_t) F_DESCRIPTOR, (jint) fd);
}
