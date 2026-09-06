// egl-config-fallback-shim.c -- FIXES-V32, 2026-09-04
//
// Идея пользователя: "нельзя вручную дать ему этот конфиг?"
//
// Проблема: и Firefox (WebRender), и Qt (qwayland-egl) на этом устройстве
// получают ноль конфигов от eglChooseConfig и откатываются на software
// ("Failed to create EGLConfig for WebRender!" / "Cannot find EGLConfig,
// returning null config"). При этом изолированный C-тест (egl-probe.c) на
// том же устройстве, с теми же переменными окружения, получает конфиг на
// ЛЮБОЙ разумный запрос -- включая RGBA8888 + depth24 + stencil8 -- и
// успешно создаёт window surface. То есть драйвер и набор конфигов в
// порядке, ломается что-то внутри самих приложений.
//
// Этот шим перехватывает eglChooseConfig: если настоящий вызов вернул
// ноль конфигов, пробуем ПОСЛЕДОВАТЕЛЬНО ослаблять запрос (сначала без
// depth/stencil, потом только renderable type) и отдаём первый рабочий.
// Приложение получает валидный конфиг вместо отказа.

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>

static FILE *logf(void)
{
    static FILE *f = NULL;
    if (!f) f = fopen("/tmp/egl-config-fallback-shim.log", "a");
    return f;
}

static void logmsg(const char *msg, int a, int b)
{
    FILE *f = logf();
    if (!f) return;
    fprintf(f, "%s (%d, %d)\n", msg, a, b);
    fflush(f);
}

typedef EGLBoolean (*real_choose_t)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                           EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    static real_choose_t real_fn = NULL;
    if (!real_fn) {
        real_fn = (real_choose_t)dlsym(RTLD_NEXT, "eglChooseConfig");
        if (!real_fn) {
            logmsg("dlsym(eglChooseConfig) FAILED", 0, 0);
            return EGL_FALSE;
        }
    }

    EGLBoolean ok = real_fn(dpy, attrib_list, configs, config_size, num_config);
    if (ok && num_config && *num_config > 0)
        return ok;  // всё в порядке, приложение получило конфиги

    logmsg("original eglChooseConfig gave nothing -> trying fallbacks",
           ok, num_config ? *num_config : -1);

    // Фолбэк 1: тот же запрос, но без depth/stencil (частая причина отказа).
    {
        EGLint relaxed[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        EGLint n = 0;
        if (real_fn(dpy, relaxed, configs, config_size, &n) && n > 0) {
            if (num_config) *num_config = n;
            logmsg("fallback 1 (RGBA8888, no depth/stencil) OK", n, 0);
            return EGL_TRUE;
        }
    }

    // Фолбэк 2: минимальный разумный запрос -- только ES2.
    {
        EGLint minimal[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };
        EGLint n = 0;
        if (real_fn(dpy, minimal, configs, config_size, &n) && n > 0) {
            if (num_config) *num_config = n;
            logmsg("fallback 2 (ES2 only) OK", n, 0);
            return EGL_TRUE;
        }
    }

    logmsg("ALL fallbacks failed", 0, 0);
    return ok;
}
