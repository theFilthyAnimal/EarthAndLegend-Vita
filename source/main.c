/*
 * Earth And Legend (PS Vita) - entry point.
 *
 * The original Android application consists of a thin Java layer around the
 * native ShiVa3D engine (libS3DClient.so). Instead of emulating the whole
 * Activity lifecycle we call the engine's JNI entry points directly from a
 * native main().
 */

#include "utils/init.h"
#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/settings.h"
#include "utils/trace.h"
#include "utils/utils.h"

#include "engine.h"
#include "java.h"
#include "reimpl/controls.h"
#include "reimpl/io.h"
#include "reimpl/net.h"
#include "reimpl/openal.h"
#include "reimpl/present.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>

#include <vitaGL.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#define SCREEN_WIDTH  960
#define SCREEN_HEIGHT 544

/* Texture decode / upload statistics gathered by the import wrappers. */
void eal_dxt_take_stats(uint64_t *time_us, unsigned *count);
void eal_etc1_take_stats(uint64_t *time_us, unsigned *count,
                         uint64_t *bytes, unsigned *mips_skipped);
void eal_io_take_read_stats(uint64_t *bytes, uint64_t *time_us,
                            unsigned *pack_misses, uint64_t *pack_time_us);

/* Set by the Android-log bridge when the engine announces its shutdown. */
extern volatile int eal_quit_requested;

int _newlib_heap_size_user = 320 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;

/*
 * Touch state delivered to the engine.
 *
 * The original Java code packs the active pointers into the first N slots of
 * a five-slot array and zeroes the rest. We do the same: real touch screen
 * fingers and the synthesized left/right stick pointers share one compacted
 * list.
 */
#define MAX_POINTERS 5

typedef struct {
    int32_t id;
    float   x;
    float   y;
} TouchPointer;

static TouchPointer pointers[MAX_POINTERS];
static int pointer_count = 0;
static int real_pointer_count = 0;

static bool  touch_dirty = false;
static bool  mouse_active = false;
static float last_mouse_x = -1.0f;
static float last_mouse_y = -1.0f;

static bool pointer_is_real(int32_t id) {
    return id >= 0;
}

static int pointer_index(int32_t id) {
    for (int i = 0; i < pointer_count; i++) {
        if (pointers[i].id == id)
            return i;
    }
    return -1;
}

static void pointer_add(int32_t id, float x, float y) {
    if (pointer_count >= MAX_POINTERS)
        return;

    pointers[pointer_count].id = id;
    pointers[pointer_count].x = x;
    pointers[pointer_count].y = y;
    pointer_count++;

    if (pointer_is_real(id))
        real_pointer_count++;
}

static void pointer_remove_at(int idx) {
    if (idx < 0 || idx >= pointer_count)
        return;

    if (pointer_is_real(pointers[idx].id))
        real_pointer_count--;

    for (int i = idx; i < pointer_count - 1; i++)
        pointers[i] = pointers[i + 1];

    pointer_count--;
}

static void pointer_set(int idx, float x, float y) {
    pointers[idx].x = x;
    pointers[idx].y = y;
}

static void touch_flush(void) {
    if (!touch_dirty)
        return;

    float active[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float x[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float y[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < pointer_count; i++) {
        active[i] = 1.0f;
        x[i] = pointers[i].x;
        y[i] = pointers[i].y;
    }

    engine_on_touches_change(active, x, y);
    touch_dirty = false;
}

/*
 * Controls callbacks.
 */

void controls_handler_key(int32_t keycode, ControlsAction action) {
    if (action == CONTROLS_ACTION_DOWN) {
        eal_trace("key: down %d", keycode);
        engine_on_keyboard_key_down(keycode, 0);
    } else if (action == CONTROLS_ACTION_UP) {
        eal_trace("key: up %d", keycode);
        engine_on_keyboard_key_up(keycode, 0);
    }
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    int idx = pointer_index(id);

    switch (action) {
        case CONTROLS_ACTION_DOWN: {
            if (idx >= 0) {
                if (pointers[idx].x != x || pointers[idx].y != y) {
                    pointer_set(idx, x, y);
                    touch_dirty = true;
                }
                break;
            }

            if (pointer_count >= MAX_POINTERS)
                break;

            eal_trace("touch: down id=%d (%.0f,%.0f)", id, x, y);
            pointer_add(id, x, y);

            // The original Android code emulates a mouse button for a single
            // finger. Virtual stick pointers do not affect this.
            if (real_pointer_count == 1 && !mouse_active) {
                engine_on_mouse_button_down(x, y);
                mouse_active = true;
                last_mouse_x = x;
                last_mouse_y = y;
            }

            touch_dirty = true;
            break;
        }

        case CONTROLS_ACTION_MOVE: {
            if (idx < 0)
                break;

            if (pointers[idx].x == x && pointers[idx].y == y)
                break;

            pointer_set(idx, x, y);

            if (real_pointer_count == 1 && mouse_active &&
                (x != last_mouse_x || y != last_mouse_y)) {
                engine_on_mouse_move(x, y);
                last_mouse_x = x;
                last_mouse_y = y;
            }

            touch_dirty = true;
            break;
        }

        case CONTROLS_ACTION_UP: {
            if (idx < 0)
                break;

            eal_trace("touch: up id=%d (%.0f,%.0f)", id, x, y);
            pointer_remove_at(idx);

            if (real_pointer_count == 0 && mouse_active) {
                engine_on_mouse_button_up(x, y);
                mouse_active = false;
                last_mouse_x = -1.0f;
                last_mouse_y = -1.0f;
            }

            touch_dirty = true;
            break;
        }
    }
}

void controls_handler_virtual_touch(int32_t id, float x, float y,
                                    ControlsAction action) {
    int idx = pointer_index(id);

    if (action == CONTROLS_ACTION_UP) {
        if (idx >= 0) {
            pointer_remove_at(idx);
            touch_dirty = true;
        }
        return;
    }

    if (idx >= 0) {
        if (pointers[idx].x != x || pointers[idx].y != y) {
            pointer_set(idx, x, y);
            touch_dirty = true;
        }
        return;
    }

    if (pointer_count >= MAX_POINTERS)
        return;

    pointer_add(id, x, y);
    touch_dirty = true;
}

void controls_handler_mouse(float x, float y, ControlsAction action) {
    if (action == CONTROLS_ACTION_DOWN) {
        eal_trace("mouse: down (%.0f,%.0f)", x, y);
        engine_on_mouse_button_down(x, y);
    } else if (action == CONTROLS_ACTION_UP) {
        eal_trace("mouse: up (%.0f,%.0f)", x, y);
        engine_on_mouse_button_up(x, y);
    } else {
        engine_on_mouse_move(x, y);
    }
}

/*
 * Initialisation helpers.
 */

static void create_data_directories(void) {
    mkdir(DATA_PATH "cache", 0777);
    mkdir(DATA_PATH "home", 0777);
    mkdir(DATA_PATH "fonts", 0777);
    mkdir(DATA_PATH "Cache", 0777);
    mkdir(DATA_PATH "Home", 0777);
    mkdir(DATA_PATH "Fonts", 0777);
    int logs_res = mkdir(DATA_PATH "Logs", 0777);
    eal_trace("engine: mkdir Logs -> %d", logs_res);

    /* 3.0.0 stores profiles relative to the working directory. */
    mkdir(DATA_PATH "Saves", 0777);
    mkdir(DATA_PATH "saves", 0777);
    mkdir(DATA_PATH "home/Saves", 0777);
    mkdir(DATA_PATH "home/saves", 0777);
    mkdir(DATA_PATH "cache/Saves", 0777);

    /* Compiled GLSL shader cache. Without these directories the engine
     * recompiles every shader on each scene load (long black screens). */
    mkdir(DATA_PATH "Cache/Shaders", 0777);
    mkdir(DATA_PATH "cache/Shaders", 0777);
    mkdir(DATA_PATH "home/Cache/Shaders", 0777);

    /* The engine creates some folders with relative names ("Cache"); make the
     * data directory the working directory so those land in a writable place
     * instead of the read-only app0: root. */
    if (chdir(DATA_PATH) == 0) {
        char cwd[128];
        if (getcwd(cwd, sizeof(cwd)))
            eal_trace("engine: working directory %s", cwd);
    } else {
        eal_trace("engine: chdir(%s) failed", DATA_PATH);
    }
}

static void engine_configure(void) {
    engine_set_camera_device_count(1);
    engine_set_camera_device_name(0, "Default");
    engine_set_device_model("PlayStation Vita");
    engine_set_system_version("4.4.2");
    engine_set_system_language("en_US");

    // The original Java code passes absolute directory paths without a
    // trailing slash; the engine appends "/S3DMain.stk" itself.
    static char pack_dir[256];
    snprintf(pack_dir, sizeof(pack_dir), "%s", DATA_PATH);

    size_t pack_len = strlen(pack_dir);
    if (pack_len > 0 && pack_dir[pack_len - 1] == '/')
        pack_dir[pack_len - 1] = '\0';

    engine_set_directories(DATA_PATH "cache", DATA_PATH "home", pack_dir);

    int fd = open(DATA_PATH "S3DMain.stk", O_RDONLY);
    size_t size = file_size(DATA_PATH "S3DMain.stk");

    if (fd >= 0 && size != (size_t) -1) {
        l_success("Data pack opened, fd %i, size %u", fd, (unsigned) size);

        /* 3.0.0 treats the descriptor as an Android APK descriptor
         * (assets.openFd) and then tries to read the pack through it. Our
         * file is a standalone pack, so let the engine open the path from
         * the filesystem instead. */
        if (engine_is_v3()) {
            /* Force the filesystem path: an invalid descriptor makes the
             * engine skip the APK-descriptor branch and open the .stk file. */
            engine_set_pack_file_descriptor(-1, 0, 0);
            eal_trace("engine: v3 pack fd invalidated (filesystem path)");
        } else {
            engine_set_pack_file_descriptor(fd, 0, (long) size);
        }
    } else {
        l_warn("Could not open data pack %sS3DMain.stk", DATA_PATH);
    }

    engine_set_location_support(false);
    engine_set_heading_support(false);

    char ip[32] = "127.0.0.1";
    char local_ip[32];

    if (eal_net_local_ip(local_ip, sizeof(local_ip))) {
        strncpy(ip, local_ip, sizeof(ip) - 1);
        ip[sizeof(ip) - 1] = '\0';
    }

    eal_trace("engine: device IP %s", ip);
    engine_set_device_ip_address(ip);
    engine_set_device_uuid("vita-earth-and-legend");
}

static void handle_engine_state(void) {
    if (engine_get_overlay_movie_has_changed()) {
        const char * movie = engine_get_overlay_movie();
        l_debug("Overlay movie: '%s'", movie);
        eal_trace("engine: overlay movie changed: '%s'", movie);

        // We cannot play videos; tell the engine the movie is over so it
        // does not wait for the Java side.
        if (movie[0] != '\0')
            engine_on_overlay_movie_stopped();
    }

    (void) engine_get_camera_device_state();
}

static void pump_engine_log(void) {
    static long pos = 0;

    FILE *f = fopen(DATA_PATH "S3DClient.log", "r");
    if (!f)
        return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < pos)
        pos = 0;
    fseek(f, pos, SEEK_SET);

    char line[512];
    int count = 0;

    while (count < 120 && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len > 0)
            eal_trace("s3d: %s", line);
        count++;
        pos = ftell(f);
    }

    fclose(f);
}

static void dump_engine_log(void) {
    FILE *f = fopen(DATA_PATH "S3DClient.log", "r");
    if (!f) {
        eal_trace("s3d: engine log not found");
        return;
    }

    char line[512];
    int count = 0;

    while (count < 300 && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len > 0)
            eal_trace("s3d: %s", line);
        count++;
    }

    fclose(f);
    eal_trace("s3d: engine log dumped (%d lines)", count);
}

static int engine_thread(SceSize args, void *argp) {
    (void) args;
    (void) argp;

    eal_io_init();
    eal_trace("=== Earth And Legend boot ===");

    soloader_init_all();

    eal_trace("main: CPU cores reported to the engine: %d",
              setting_cpuCount > 0 ? setting_cpuCount : 3);

    jint (*JNI_OnLoad)(JavaVM *, void *) =
        (void *) so_symbol(&so_mod, "JNI_OnLoad");

    if (!JNI_OnLoad) {
        l_fatal("JNI_OnLoad not found in %s", SO_PATH);
        eal_trace("fatal: JNI_OnLoad missing in %s", SO_PATH);
        fatal_error("Error: JNI_OnLoad is missing from the game library.");
    }

    JNI_OnLoad(&jvm, NULL);
    eal_trace("JNI_OnLoad returned");

    java_init_values();

    if (!engine_init()) {
        eal_trace("fatal: engine symbols missing");
        fatal_error("Error: the game library is missing engine symbols.");
    }

    engine_apply_cpu_count(setting_cpuCount > 0 ? setting_cpuCount : 3);

    // Audio: when enabled, the OpenAL backend is selected after the engine
    // has created its core (see engine_audio_start() below). Calling
    // S3DClient_SetAudioDisabled() forces driver 0, so only use it to keep
    // audio fully off.
    if (!setting_openal) {
        engine_disable_audio(true);
        eal_trace("engine: audio disabled (pre-init)");
    }

    gl_init();
    vglWaitVblankStart(setting_vsync ? GL_TRUE : GL_FALSE);
    eal_trace("vitaGL initialised (vsync=%d)", setting_vsync);
    present_init();

    create_data_directories();
    engine_configure();

    engine_on_surface_created();
    engine_on_surface_changed(present_engine_width(), present_engine_height());

    engine_shutdown();

    if (!engine_initialize()) {
        l_fatal("engineInitialize() failed.");
        eal_trace("fatal: engineInitialize failed");
        fatal_error("Error: the game engine failed to initialize.");
    }

    engine_apply_cpu_count(setting_cpuCount > 0 ? setting_cpuCount : 3);

    // The first call can be a no-op if the engine core is not ready yet.
    if (!setting_openal) {
        engine_disable_audio(true);
        eal_trace("engine: audio disabled (post-init)");
    }

    eal_trace("engine: default viewport rotation %d",
              engine_get_viewport_rotation());
    engine_on_surface_changed(present_engine_width(), present_engine_height());

    if (setting_openal)
        engine_audio_start();

    if (engine_is_v3()) {
        engine_v3_start(DATA_PATH "S3DMain.stk");

        if (!setting_v3Sync)
            engine_async_start();
    }

    eal_trace("engine: initialised (viewport rotation %d)",
              engine_get_viewport_rotation());

    l_success("Engine initialized, entering main loop.");

    bool running = true;
    unsigned int frame = 0;
    bool v3 = engine_is_v3();
    bool v3_async = v3 && !setting_v3Sync;

    /* Performance counters: report once every five seconds. */
    uint64_t stats_start = sceKernelGetProcessTimeWide();
    uint64_t stats_engine_us = 0;
    uint64_t stats_present_us = 0;
    unsigned stats_frames = 0;
    unsigned stats_draws = 0;
    uint64_t v3_prev_run = 0;
    uint64_t v3_prev_render = 0;

    while (running) {
        uint64_t t_begin = sceKernelGetProcessTimeWide();

        controls_poll();
        touch_flush();
        present_begin_frame();

        bool want_swap = false;

        if (v3_async) {
            bool rendered = engine_run_one_frame();
            if (!rendered) {
                static int skipped_async = 0;
                if (skipped_async < 8) {
                    eal_trace("engine: v3 async frame not ready");
                    skipped_async++;
                }
            }

            handle_engine_state();
            engine_async_flush();

            if (eal_quit_requested) {
                eal_trace("engine: quit requested, ending the loop");
                running = false;
            }

            if (engine_v3_stopped()) {
                eal_trace("engine: v3 engine stopped, ending the loop");
                running = false;
            }

            want_swap = true;
        } else if (v3) {
            bool rendered = engine_v3_frame() != 0;
            if (!rendered) {
                static int skipped_frames = 0;
                if (skipped_frames < 8) {
                    eal_trace("engine: v3 frame not ready");
                    skipped_frames++;
                }
            }

            handle_engine_state();

            if (eal_quit_requested) {
                eal_trace("engine: quit requested, ending the loop");
                running = false;
            }

            if (engine_v3_stopped()) {
                eal_trace("engine: v3 engine stopped, ending the loop");
                running = false;
            }

            want_swap = true;
        } else {
            do {
                if (!engine_run_one_frame()) {
                    l_success("Engine requested shutdown.");
                    eal_trace("engine: runOneFrame requested shutdown at frame %u",
                              frame);
                    running = false;
                    break;
                }

                handle_engine_state();

                want_swap = engine_get_want_swap_buffers();

                /* No glFinish() here: it forced a full GPU sync for every
                 * internal engine frame and caused large stalls. Nothing reads
                 * the framebuffer between frames, so the sync is unnecessary. */
            } while (!want_swap && running);
        }

        if (!running)
            break;

        uint64_t t_engine = sceKernelGetProcessTimeWide();

        present_frame();
        gl_swap();

        uint64_t t_present = sceKernelGetProcessTimeWide();

        stats_engine_us += t_engine - t_begin;
        stats_present_us += t_present - t_engine;
        stats_frames++;
        stats_draws += present_take_draw_calls();

        if (t_present - stats_start >= 5000000ull) {
            double seconds = (double) (t_present - stats_start) / 1000000.0;
            eal_trace("perf: %.1f fps, engine %.1f ms, present %.1f ms, "
                      "%.0f draws/frame",
                      (double) stats_frames / seconds,
                      (double) stats_engine_us / (1000.0 * stats_frames),
                      (double) stats_present_us / (1000.0 * stats_frames),
                      (double) stats_draws / (double) stats_frames);

            uint64_t dxt_us = 0;
            unsigned dxt_n = 0;
            eal_dxt_take_stats(&dxt_us, &dxt_n);
            if (dxt_n > 0) {
                eal_trace("perf: dxt decode %.1f ms total, %u textures",
                          (double) dxt_us / 1000.0, dxt_n);
            }

            uint64_t etc1_us = 0, etc1_bytes = 0;
            unsigned etc1_n = 0, mips_skipped = 0;
            eal_etc1_take_stats(&etc1_us, &etc1_n, &etc1_bytes, &mips_skipped);
            if (etc1_n > 0 || mips_skipped > 0) {
                eal_trace("perf: etc1 upload %.1f ms, %u tex, %.2f MB, "
                          "%u mips skipped",
                          (double) etc1_us / 1000.0, etc1_n,
                          (double) etc1_bytes / (1024.0 * 1024.0),
                          mips_skipped);
            }

            uint64_t io_bytes = 0, io_us = 0, pack_us = 0;
            unsigned pack_misses = 0;
            eal_io_take_read_stats(&io_bytes, &io_us, &pack_misses, &pack_us);
            if (io_bytes > 0 || pack_misses > 0) {
                eal_trace("perf: file read %.2f MB in %.1f ms, "
                          "pack %u blocks / %.1f ms",
                          (double) io_bytes / (1024.0 * 1024.0),
                          (double) io_us / 1000.0,
                          pack_misses,
                          (double) pack_us / 1000.0);
            }

            if (v3 && stats_frames > 0) {
                uint64_t run_us = 0, render_us = 0;
                engine_v3_get_times(&run_us, &render_us);
                eal_trace("perf: v3 logic=%.1f ms, render=%.1f ms",
                          (double) (run_us - v3_prev_run) /
                          (1000.0 * stats_frames),
                          (double) (render_us - v3_prev_render) /
                          (1000.0 * stats_frames));
                v3_prev_run = run_us;
                v3_prev_render = render_us;
            }

            stats_start = t_present;
            stats_engine_us = 0;
            stats_present_us = 0;
            stats_frames = 0;
            stats_draws = 0;

            SceKernelFreeMemorySizeInfo meminfo;
            meminfo.size = sizeof(meminfo);
            if (sceKernelGetFreeMemorySize(&meminfo) == 0) {
                eal_trace("mem: free user=%dK cdram=%dK phycont=%dK",
                          (int) meminfo.size_user / 1024,
                          (int) meminfo.size_cdram / 1024,
                          (int) meminfo.size_phycont / 1024);
            }

            eal_trace("vgl: free vram=%dK ram=%dK slow=%dK",
                      (int) (vglMemFree(VGL_MEM_VRAM) / 1024),
                      (int) (vglMemFree(VGL_MEM_RAM) / 1024),
                      (int) (vglMemFree(VGL_MEM_SLOW) / 1024));

            pump_engine_log();
        }

        if (frame == 0) {
            (void) engine_did_pass_first_frame();
            eal_trace("engine: first frame presented");

            // The engine only accepts the audio driver once its core is
            // fully up; retry once in case the first attempt was too early.
            if (setting_openal && !eal_openal_active()) {
                eal_trace("engine: retrying audio device init");
                engine_audio_start();
            }
        }

        frame++;
    }

    eal_trace("engine: main loop ended");
    bool already_stopped = (engine_is_v3() && engine_v3_stopped()) ||
                            eal_quit_requested;
    if (!already_stopped) {
        if (setting_openal)
            engine_audio_stop();
        engine_shutdown();
    }
    dump_engine_log();
    eal_openal_shutdown();

    return 0;
}

int main(void) {
    eal_net_init();

    /* Run at the Vita's maximum official clocks. The default 333/166 MHz
     * profile is not enough for a full ShiVa3D scene. */
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(222);

    // The engine performs heavy recursion during combat; give it a roomier
    // stack than the loader's default main thread.
    SceUID thread = sceKernelCreateThread("eal_engine", engine_thread,
                                          0x10000100, 1024 * 1024, 0,
                                          SCE_KERNEL_CPU_MASK_USER_ALL,
                                          NULL);

    if (thread < 0) {
        eal_trace("fatal: could not create engine thread (0x%08X)", thread);
        fatal_error("Error: could not create the engine thread (0x%08X).",
                    thread);
    }

    eal_trace("main: engine thread 0x%08X created", thread);
    sceKernelStartThread(thread, 0, NULL);
    sceKernelWaitThreadEnd(thread, NULL, NULL);

    sceKernelExitProcess(0);
    return 0;
}
