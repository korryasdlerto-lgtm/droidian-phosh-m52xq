// egl-probe.c -- минимальный тест-стенд: показывает РЕАЛЬНЫЕ EGL-конфиги,
// которые выдаёт драйвер через libhybris' "wayland" платформу, и
// проверяет типичные Qt-шные запросы к eglChooseConfig, чтобы понять,
// какой именно атрибут отклоняет qwayland-egl на этом устройстве.
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-egl.h>

static struct wl_compositor *compositor = NULL;

static void registry_global(void *data, struct wl_registry *reg, uint32_t name,
                             const char *interface, uint32_t version) {
    if (!strcmp(interface, "wl_compositor"))
        compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 1 < version ? 1 : version);
}
static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name) {}
static const struct wl_registry_listener registry_listener = { registry_global, registry_global_remove };

static void dump_config(EGLDisplay dpy, EGLConfig cfg) {
    EGLint id, r, g, b, a, d, s, surf, renderable, caveat;
    eglGetConfigAttrib(dpy, cfg, EGL_CONFIG_ID, &id);
    eglGetConfigAttrib(dpy, cfg, EGL_RED_SIZE, &r);
    eglGetConfigAttrib(dpy, cfg, EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(dpy, cfg, EGL_BLUE_SIZE, &b);
    eglGetConfigAttrib(dpy, cfg, EGL_ALPHA_SIZE, &a);
    eglGetConfigAttrib(dpy, cfg, EGL_DEPTH_SIZE, &d);
    eglGetConfigAttrib(dpy, cfg, EGL_STENCIL_SIZE, &s);
    eglGetConfigAttrib(dpy, cfg, EGL_SURFACE_TYPE, &surf);
    eglGetConfigAttrib(dpy, cfg, EGL_RENDERABLE_TYPE, &renderable);
    eglGetConfigAttrib(dpy, cfg, EGL_CONFIG_CAVEAT, &caveat);
    printf("  id=%-3d RGBA=%d%d%d%d depth=%d stencil=%d surface=0x%x renderable=0x%x caveat=0x%x%s\n",
           id, r, g, b, a, d, s, surf, renderable, caveat,
           (surf & EGL_WINDOW_BIT) ? " [WINDOW]" : "");
}

int main(void) {
    printf("EGL_VENDOR env HYBRIS_EGLPLATFORM=%s EGL_PLATFORM=%s\n",
           getenv("HYBRIS_EGLPLATFORM") ? getenv("HYBRIS_EGLPLATFORM") : "(unset)",
           getenv("EGL_PLATFORM") ? getenv("EGL_PLATFORM") : "(unset)");

    struct wl_display *wl = wl_display_connect(NULL);
    if (!wl) {
        fprintf(stderr, "wl_display_connect failed\n");
        return 1;
    }
    printf("wl_display_connect OK\n");

    struct wl_registry *registry = wl_display_get_registry(wl);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(wl);
    printf("wl_compositor bound: %s\n", compositor ? "yes" : "NO -- surface test will be skipped");

    EGLDisplay dpy;
    const char *client_exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("client extensions: %s\n", client_exts ? client_exts : "(null / not supported)");

    PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplayEXT =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (getenv("USE_PLATFORM_DISPLAY") && getPlatformDisplayEXT) {
        printf("Using eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, ...)\n");
        dpy = getPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, wl, NULL);
    } else if (getenv("USE_PLATFORM_DISPLAY")) {
        printf("eglGetPlatformDisplayEXT not available via eglGetProcAddress!\n");
        dpy = EGL_NO_DISPLAY;
    } else {
        printf("Using legacy eglGetDisplay(...)\n");
        dpy = eglGetDisplay((EGLNativeDisplayType)wl);
    }
    if (dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetDisplay/eglGetPlatformDisplayEXT failed: 0x%x\n", eglGetError());
        return 1;
    }
    printf("eglGetDisplay-family call OK: %p\n", dpy);

    EGLint maj, min;
    if (!eglInitialize(dpy, &maj, &min)) {
        fprintf(stderr, "eglInitialize FAILED: 0x%x\n", eglGetError());
        return 1;
    }
    printf("eglInitialize OK: %d.%d, vendor=%s, version=%s, extensions=%s\n",
           maj, min, eglQueryString(dpy, EGL_VENDOR), eglQueryString(dpy, EGL_VERSION),
           eglQueryString(dpy, EGL_EXTENSIONS));

    EGLint num = 0;
    eglGetConfigs(dpy, NULL, 0, &num);
    printf("\nTotal configs available (eglGetConfigs, no filter): %d\n", num);
    if (num > 0) {
        EGLConfig *all = malloc(sizeof(EGLConfig) * num);
        eglGetConfigs(dpy, all, num, &num);
        for (int i = 0; i < num; i++) dump_config(dpy, all[i]);
        free(all);
    }

    // Типичный запрос, который делает Qt's qwayland-egl (QEglFSKmsGbmIntegration /
    // QEglFSIntegration стиль: RGBA 8888, EGL_WINDOW_BIT, OpenGL ES2).
    printf("\n--- eglChooseConfig, Qt-style attrs (RGBA8888, WINDOW_BIT, ES2) ---\n");
    EGLint attrs1[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig chosen;
    EGLint chosen_num = 0;
    if (eglChooseConfig(dpy, attrs1, &chosen, 1, &chosen_num) && chosen_num > 0) {
        printf("SUCCESS, got %d config(s):\n", chosen_num);
        dump_config(dpy, chosen);
    } else {
        printf("FAILED: eglChooseConfig returned nothing (num=%d), error=0x%x\n", chosen_num, eglGetError());
    }

    // То же самое, но без запроса alpha (некоторые Qt-платформы просят
    // RGB565 или RGB888 без альфы для основного окна).
    printf("\n--- eglChooseConfig, no alpha (RGB888, WINDOW_BIT, ES2) ---\n");
    EGLint attrs2[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    chosen_num = 0;
    if (eglChooseConfig(dpy, attrs2, &chosen, 1, &chosen_num) && chosen_num > 0) {
        printf("SUCCESS, got %d config(s):\n", chosen_num);
        dump_config(dpy, chosen);
    } else {
        printf("FAILED: eglChooseConfig returned nothing (num=%d), error=0x%x\n", chosen_num, eglGetError());
    }

    // Типичный запрос браузера/Qt: RGBA8888 + depth 24 + stencil 8.
    printf("\n--- eglChooseConfig, WITH depth24+stencil8 (как просят Firefox/Qt) ---\n");
    EGLint attrsDS[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    chosen_num = 0;
    if (eglChooseConfig(dpy, attrsDS, &chosen, 1, &chosen_num) && chosen_num > 0)
        printf("SUCCESS: depth24+stencil8 config found\n");
    else
        printf("FAILED: НЕТ конфига с depth24+stencil8 (num=%d, err=0x%x) <-- ВОТ ПРИЧИНА\n",
               chosen_num, eglGetError());

    // Минимальный запрос вообще без EGL_SURFACE_TYPE (context-only style).
    printf("\n--- eglChooseConfig, minimal (just ES2 renderable) ---\n");
    EGLint attrs3[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    chosen_num = 0;
    if (eglChooseConfig(dpy, attrs3, &chosen, 1, &chosen_num) && chosen_num > 0) {
        printf("SUCCESS, got %d config(s):\n", chosen_num);
        dump_config(dpy, chosen);
    } else {
        printf("FAILED: eglChooseConfig returned nothing (num=%d), error=0x%x\n", chosen_num, eglGetError());
    }

    // Настоящий тест: создать реальную wl_surface + wl_egl_window и
    // попробовать eglCreateWindowSurface -- это ТОЧНЫЙ шаг, на котором
    // падает Qt ("Could not create EGL surface (EGL error 0x3005)").
    printf("\n--- eglCreateWindowSurface (real wl_surface + wl_egl_window) ---\n");
    if (!compositor) {
        printf("SKIPPED: no wl_compositor\n");
        return 0;
    }
    struct wl_surface *surf = wl_compositor_create_surface(compositor);
    if (!surf) {
        printf("FAILED: wl_compositor_create_surface returned NULL\n");
        return 1;
    }
    printf("wl_surface created OK\n");

    int win_w = getenv("PROBE_W") ? atoi(getenv("PROBE_W")) : 400;
    int win_h = getenv("PROBE_H") ? atoi(getenv("PROBE_H")) : 400;
    printf("Creating wl_egl_window at %dx%d\n", win_w, win_h);
    struct wl_egl_window *eglwin = wl_egl_window_create(surf, win_w, win_h);
    if (!eglwin) {
        printf("FAILED: wl_egl_window_create returned NULL\n");
        return 1;
    }
    printf("wl_egl_window created OK: %p\n", eglwin);

    // используем конфиг из последнего успешного eglChooseConfig (attrs1)
    EGLSurface eglsurf = eglCreateWindowSurface(dpy, chosen, (EGLNativeWindowType)eglwin, NULL);
    if (eglsurf == EGL_NO_SURFACE) {
        printf("FAILED: eglCreateWindowSurface returned EGL_NO_SURFACE, error=0x%x\n", eglGetError());
    } else {
        printf("SUCCESS: eglCreateWindowSurface OK: %p\n", eglsurf);
    }

    return 0;
}
