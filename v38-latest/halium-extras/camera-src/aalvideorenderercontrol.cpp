/*
 * Copyright (C) 2012 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "aalvideorenderercontrol.h"
#include "aalcameraservice.h"
#include "aalviewfindersettingscontrol.h"

#include <hybris/camera/camera_compatibility_layer.h>
#include <hybris/camera/camera_compatibility_layer_capabilities.h>
#include <qtubuntu_media_signals.h>

#include <QAbstractVideoBuffer>
#include <QAbstractVideoSurface>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QVideoSurfaceFormat>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <dlfcn.h>
#include <cstring>
#include <atomic>

// CLAUDE_DEBUG 2026-09-05: диагностический счётчик реальных кадров от
// HAL, инкрементируется в onPreviewFrameCB() (поток колбэка -- НИКАКИХ
// qWarning() там, см. ФИКСЫ_ДЛЯ_В32.txt п.6.7). Читается и печатается
// только из updateViewfinderFrame() (GUI-поток), чтобы понять, реально
// ли идёт поток, раз внутри самого колбэка логировать нельзя.
static std::atomic<int> g_previewFrameCount(0);

class AalTextureBufferMapper {
public:
    AalTextureBufferMapper() :
        m_textureId(0),
        m_width(0),
        m_height(0),
        m_mapMode(QAbstractVideoBuffer::NotMapped)
    {}
    virtual ~AalTextureBufferMapper() {}

    void setTextureId(GLuint textureId) { m_textureId = textureId; }
    virtual void setSize(const QSize& size)
    {
        m_width = size.width();
        m_height = size.height();
    }

    virtual QAbstractVideoBuffer::MapMode mapMode() const = 0;
    virtual uchar* map(QAbstractVideoBuffer::MapMode mode, int* numBytes, int* bytesPerLine) = 0;
    virtual void unmap() = 0;

    virtual QAbstractVideoBuffer::HandleType handleType() const
    {
        return QAbstractVideoBuffer::NoHandle;
    }

    // CLAUDE_DEBUG 2026-09-05 (ФИКСЫ_ДЛЯ_В32.txt п.6.7): вызывается из
    // onPreviewFrameCB() с сырым NV21-буфером HAL. Может звонить с ЛЮБОГО
    // потока -- реализация сама берёт лок. Дефолт-заглушка на случай
    // будущих мапперов, которые не читают NV21 напрямую.
    virtual void updateFromNV21(const uint8_t* /*data*/, uint32_t /*dataSize*/) {}

protected:
    GLuint m_textureId;
    int m_width;
    int m_height;
    QAbstractVideoBuffer::MapMode m_mapMode;
};

class AalCallbackFrameMapper : public AalTextureBufferMapper {
public:
    AalCallbackFrameMapper() :
        AalTextureBufferMapper(),
        m_buffer(nullptr),
        m_bufferBytes(0),
        m_mapScratch(nullptr),
        m_mapScratchBytes(0)
    {
    }

    ~AalCallbackFrameMapper()
    {
        delete[] m_buffer;
        delete[] m_mapScratch;
    }

    QAbstractVideoBuffer::MapMode mapMode() const override
    {
        return m_mapMode;
    }

    QAbstractVideoBuffer::HandleType handleType() const override
    {
        return QAbstractVideoBuffer::NoHandle;
    }

    // CLAUDE_DEBUG 2026-09-05: setSize() зовётся из GUI-потока
    // (updateViewfinderFrame()), а m_width/m_height читаются из потока
    // HAL внутри updateFromNV21() -- без общего лока это чистый data
    // race (могли увидеть несогласованный размер посреди записи, отсюда
    // "stack corruption detected" при первом же реальном кадре). Тем же
    // m_mutex синхронизируем запись размера с чтением/конвертацией.
    void setSize(const QSize& size) override
    {
        QMutexLocker lock(&m_mutex);
        AalTextureBufferMapper::setSize(size);
    }

    // CLAUDE_DEBUG 2026-09-05 (ФИКСЫ_ДЛЯ_В32.txt п.6.7): ночной успешный
    // поток шёл именно через callback-режим Camera1 API (сырые NV21-кадры
    // через C-колбэк), а не через GL-текстуру -- этот код для GL-текстуры
    // так и не смог отрендериться под QT_QUICK_BACKEND=software. Здесь --
    // прямая конвертация NV21 (Y-плоскость + перемежённая VU, 4:2:0) в
    // RGB32 (BT.601, тот же алгоритм, что в стандартных Android
    // YUV-конвертерах), без единого вызова GL/EGL. Лочится на время
    // конвертации, чтобы map()/unmap() не читали буфер наполовину
    // обновлённым.
    void updateFromNV21(const uint8_t* data, uint32_t dataSize) override
    {
        QMutexLocker lock(&m_mutex);

        if (m_width <= 0 || m_height <= 0 || !data) {
            return;
        }

        // CLAUDE_DEBUG 2026-09-05: НЕ логировать здесь (qWarning()) --
        // эта функция зовётся из потока HAL-колбэка, см. ФИКСЫ_ДЛЯ_В32.txt
        // п.6.7 про "stack corruption" от qWarning() в этом контексте.
        const size_t expectedBytes = size_t(m_width) * size_t(m_height) * 3 / 2;
        if (dataSize < expectedBytes) {
            return;
        }

        const size_t requiredBytes = size_t(m_width) * size_t(m_height) * 4;
        if (m_buffer && m_bufferBytes != requiredBytes) {
            delete[] m_buffer;
            m_buffer = nullptr;
        }
        if (!m_buffer) {
            m_buffer = new uint8_t[requiredBytes];
            m_bufferBytes = requiredBytes;
        }

        const uint8_t* yPlane = data;
        const uint8_t* uvPlane = data + size_t(m_width) * size_t(m_height);

        for (int y = 0; y < m_height; ++y) {
            const uint8_t* yRow = yPlane + size_t(y) * m_width;
            const uint8_t* uvRow = uvPlane + size_t(y / 2) * m_width;
            uint8_t* outRow = m_buffer + size_t(y) * m_width * 4;

            for (int x = 0; x < m_width; ++x) {
                const int Y = yRow[x];
                const int uvIndex = (x / 2) * 2;
                // NV21: перемежённые V,U (в этом порядке), полу-разрешение.
                const int V = int(uvRow[uvIndex])     - 128;
                const int U = int(uvRow[uvIndex + 1]) - 128;

                const int C = Y - 16;
                const int r = (298 * C + 409 * V + 128) >> 8;
                const int g = (298 * C - 100 * U - 208 * V + 128) >> 8;
                const int b = (298 * C + 516 * U + 128) >> 8;

                uint8_t* px = outRow + size_t(x) * 4;
                px[0] = clampByte(b);
                px[1] = clampByte(g);
                px[2] = clampByte(r);
                px[3] = 0xFF;
            }
        }
    }

    uchar* map(QAbstractVideoBuffer::MapMode mode, int* numBytes, int* bytesPerLine) override
    {
        if (mode != QAbstractVideoBuffer::ReadOnly) {
            qWarning() << "Tried to map in unsupported mode:" << mode;
            return nullptr;
        }

        // CLAUDE_DEBUG 2026-09-05: раньше лок держался МЕЖДУ map() и
        // unmap() -- если Qt хоть раз вызовет unmap() без парного map()
        // (m_mapper -- один переиспользуемый объект на ВСЕ кадры, а
        // QVideoFrame у Qt может жить дольше одного цикла), это unlock()
        // незалоченного QMutex -- undefined behaviour (подозреваемый
        // источник "stack corruption detected" ровно на первом реальном
        // кадре). Теперь лок берётся только на время короткого memcpy в
        // отдельный scratch-буфер, unmap() не трогает мьютекс вообще.
        QMutexLocker lock(&m_mutex);
        if (!m_buffer) {
            return nullptr;
        }

        if (m_mapScratchBytes != m_bufferBytes) {
            delete[] m_mapScratch;
            m_mapScratch = new uint8_t[m_bufferBytes];
            m_mapScratchBytes = m_bufferBytes;
        }
        memcpy(m_mapScratch, m_buffer, m_bufferBytes);

        m_mapMode = mode;
        *numBytes = int(m_bufferBytes);
        *bytesPerLine = m_width * 4;
        return m_mapScratch;
    }

    void unmap() override
    {
        m_mapMode = QAbstractVideoBuffer::NotMapped;
    }

private:
    static inline uint8_t clampByte(int v)
    {
        return uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v));
    }

    QMutex m_mutex;
    uint8_t* m_buffer;
    size_t m_bufferBytes;
    uint8_t* m_mapScratch;
    size_t m_mapScratchBytes;
};

class AalGLTextureBuffer : public QAbstractVideoBuffer
{
public:
    AalGLTextureBuffer(GLuint textureId, AalTextureBufferMapper* mapper) :
        QAbstractVideoBuffer(mapper ? mapper->handleType() : QAbstractVideoBuffer::NoHandle),
        m_textureId(textureId),
        m_mapper(mapper)
    {
    }

    ~AalGLTextureBuffer()
    {
    }

    MapMode mapMode() const
    {
        if (!m_mapper)
            return QAbstractVideoBuffer::NotMapped;
        return m_mapper->mapMode();
    }

    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
    {
        if (!m_mapper)
            return nullptr;
        return m_mapper->map(mode, numBytes, bytesPerLine);
    }

    void unmap()
    {
        if (!m_mapper)
            return;
        m_mapper->unmap();
    }

    QVariant handle() const
    {
        return QVariant::fromValue<unsigned int>(m_textureId);
    }

    GLuint textureId() { return m_textureId; }

private:
    GLuint m_textureId;
    AalTextureBufferMapper* m_mapper;
};


AalVideoRendererControl::AalVideoRendererControl(AalCameraService *service, QObject *parent)
    : QVideoRendererControl(parent)
    , m_surface(0),
      m_service(service),
      m_viewFinderRunning(false),
      m_previewStarted(false),
      m_textureId(0),
      m_privateGlReady(false),
      m_eglDpy(nullptr),
      m_eglCtx(nullptr),
      m_eglSurf(nullptr)
{
    // CLAUDE_DEBUG 2026-09-05 (ФИКСЫ_ДЛЯ_В32.txt п.6.7): callback-режим
    // Camera1 API -- см. AalCallbackFrameMapper выше. Заменяет прежние
    // GL-текстурные мапперы (EGLImage и glReadPixels-с-шейдером), которые
    // требовали живого QOpenGLContext на самой сцене Qt Quick -- а его на
    // этом устройстве под QT_QUICK_BACKEND=software нет и не будет.
    m_mapper = new AalCallbackFrameMapper();

    // Get notified when qtvideo-node creates a GL texture
    connect(SharedSignal::instance(), SIGNAL(textureCreated(unsigned int)), this, SLOT(onTextureCreated(unsigned int)));
    connect(SharedSignal::instance(), SIGNAL(snapshotTaken(QImage)), this, SLOT(onSnapshotTaken(QImage)));
}

AalVideoRendererControl::~AalVideoRendererControl()
{
    if (m_mapper) {
        delete m_mapper;
        m_mapper = nullptr;
    }
}

QAbstractVideoSurface *AalVideoRendererControl::surface() const
{
    return m_surface;
}

void AalVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface != surface) {
        m_surface = surface;
        Q_EMIT surfaceChanged(surface);
    }
}

void AalVideoRendererControl::init(CameraControl *control, CameraControlListener *listener)
{
    Q_UNUSED(control);
    listener->on_preview_texture_needs_update_cb = &AalVideoRendererControl::updateViewfinderFrameCB;
    // CLAUDE_DEBUG 2026-09-05 (ФИКСЫ_ДЛЯ_В32.txt п.6.7): реальная доставка
    // кадров теперь идёт через это -- сырой NV21 колбэком, а не через
    // текстуру (см. onPreviewFrameCB() и startPreview() ниже).
    listener->on_preview_frame_cb = &AalVideoRendererControl::onPreviewFrameCB;
    // ensures a new texture will be created by qtvideo-node
    m_textureId = 0;
}

// CLAUDE_DEBUG 2026-09-04: см. .h -- создаёт СВОЙ приватный EGL-контекст
// и текстуру, полностью в обход Qt Quick (которое не может получить
// EGLConfig на этом устройстве). Проверено отдельным тест-стендом
// (egl-probe.c): сырой EGL на этом устройстве полностью рабочий.
bool AalVideoRendererControl::ensurePrivateGlContext()
{
    if (m_privateGlReady && m_textureId)
        return true;

    setenv("HYBRIS_EGLPLATFORM", "wayland", 1);

    typedef void *(*wl_display_connect_fn)(const char *);
    wl_display_connect_fn real_wl_display_connect =
        (wl_display_connect_fn)dlsym(RTLD_DEFAULT, "wl_display_connect");
    void *wlDisplay = real_wl_display_connect ? real_wl_display_connect(nullptr) : nullptr;

    EGLDisplay dpy = eglGetDisplay(wlDisplay ? (EGLNativeDisplayType)wlDisplay : EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) {
        qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() eglGetDisplay failed";
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(dpy, &major, &minor)) {
        qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() eglInitialize failed, err=" << eglGetError();
        return false;
    }

    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(dpy, configAttribs, &config, 1, &numConfigs) || numConfigs < 1) {
        qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() eglChooseConfig failed, err=" << eglGetError();
        return false;
    }

    EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, ctxAttribs);
    if (ctx == EGL_NO_CONTEXT) {
        qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() eglCreateContext failed, err=" << eglGetError();
        return false;
    }

    EGLint pbufAttribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface surf = eglCreatePbufferSurface(dpy, config, pbufAttribs);
    if (surf == EGL_NO_SURFACE) {
        qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() eglCreatePbufferSurface failed, err=" << eglGetError();
        eglDestroyContext(dpy, ctx);
        return false;
    }

    if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
        qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() eglMakeCurrent failed, err=" << eglGetError();
        eglDestroySurface(dpy, surf);
        eglDestroyContext(dpy, ctx);
        return false;
    }

    typedef void (*glGenTextures_fn)(GLsizei, GLuint*);
    glGenTextures_fn realGlGenTextures = (glGenTextures_fn)eglGetProcAddress("glGenTextures");
    if (!realGlGenTextures) {
        qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() eglGetProcAddress(glGenTextures) failed";
        return false;
    }
    GLuint tex = 0;
    realGlGenTextures(1, &tex);
    if (!tex) {
        qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() glGenTextures returned 0";
        return false;
    }

    m_eglDpy = dpy;
    m_eglCtx = ctx;
    m_eglSurf = surf;
    m_textureId = tex;
    m_privateGlReady = true;

    qWarning() << "CLAUDE_DEBUG: ensurePrivateGlContext() OK, texture id=" << m_textureId;
    return true;
}

void AalVideoRendererControl::startPreview()
{
    if (m_previewStarted) {
        return;
    }
    if (!m_service->androidControl()) {
        qWarning() << "Can't start preview without a CameraControl";
        return;
    }
    m_previewStarted = true;

    // CLAUDE_DEBUG 2026-09-04: на штатном пути m_textureId остаётся 0,
    // потому что Qt Quick не может создать текстуру на этом устройстве
    // (см. .h) -- без этого android_camera_start_preview() не вызывался
    // бы вообще. Даём HAL свой приватный текстуру заранее.
    if (!m_textureId) {
        ensurePrivateGlContext();
    }

    if (m_textureId) {
        CameraControl *cc = m_service->androidControl();
        // CLAUDE_DEBUG 2026-09-05 (ФИКСЫ_ДЛЯ_В32.txt п.6.7): текстура
        // по-прежнему нужна ТОЛЬКО чтобы HAL считал preview window
        // валидным (haveValidPreviewWindow() в Camera2Client.cpp иначе
        // вообще не начинает стримить, даже в CPU-режиме кадров) -- сами
        // пиксели через неё больше не идут, реальная доставка -- ниже,
        // через callback-режим (тот же путь, что дал 1261+ кадр живьём
        // ночью 2026-09-04).
        android_camera_set_preview_texture(cc, m_textureId);
        android_camera_set_preview_callback_mode(cc, PREVIEW_CALLBACK_ENABLED);
        android_camera_start_preview(cc);
        qWarning() << "CLAUDE_DEBUG: startPreview() called android_camera_start_preview() (callback mode), textureId=" << m_textureId;
    } else {
        qWarning() << "CLAUDE_DEBUG: startPreview() NO texture, HAL preview NOT started";
    }

    // if no texture ID is set to the frame passed to ShaderVideoNode,
    // a texture ID will be generated and returned via the 'textureCreated' signal
    // after calling updateViewfinderFrame()
    updateViewfinderFrame();

    m_service->updateCaptureReady();
}

void AalVideoRendererControl::stopPreview()
{
    if (!m_previewStarted) {
        return;
    }
    if (!m_service->androidControl()) {
        qWarning() << "Can't stop preview without a CameraControl";
        return;
    }
    if (!m_surface) {
        qWarning() << "Can't stop preview without a QAbstractVideoSurface";
        return;
    }

    if (m_surface->isActive()) {
        m_surface->stop();
    }

    CameraControl *cc = m_service->androidControl();
    android_camera_set_preview_callback_mode(cc, PREVIEW_CALLBACK_DISABLED);
    android_camera_stop_preview(cc);
    // FIXME: missing android_camera_set_preview_size(QSize())
    android_camera_set_preview_texture(cc, 0);

    m_previewStarted = false;
    m_service->updateCaptureReady();
}

bool AalVideoRendererControl::isPreviewStarted() const
{
    return m_previewStarted;
}

void AalVideoRendererControl::updateViewfinderFrame()
{
    static int s_callCount = 0;
    ++s_callCount;
    if (s_callCount <= 5 || s_callCount % 30 == 0) {
        qWarning() << "CLAUDE_DEBUG: updateViewfinderFrame() called #" << s_callCount
                   << "realHalFrames=" << g_previewFrameCount.load(std::memory_order_relaxed);
    }

    if (!m_service->viewfinderControl()) {
        qWarning() << "Can't draw video frame without a viewfinder settings control";
        return;
    }
    if (!m_service->androidControl()) {
        qWarning() << "Can't draw video frame without camera";
        return;
    }
    if (!m_surface) {
        qWarning() << "Can't draw video frame without surface";
        return;
    }

    QSize vfSize = m_service->viewfinderControl()->currentSize();
    m_mapper->setTextureId(m_textureId);
    m_mapper->setSize(vfSize);
    QVideoFrame frame(new AalGLTextureBuffer(m_textureId, m_mapper), vfSize, QVideoFrame::Format_RGB32);

    if (!frame.isValid()) {
        qWarning() << "Invalid frame";
        return;
    }

    CameraControl *cc = m_service->androidControl();
    frame.setMetaData("CamControl", QVariant::fromValue((void*)cc));

    if (!m_surface->isActive()) {
        QVideoSurfaceFormat format(frame.size(), frame.pixelFormat(), frame.handleType());

        if (!m_surface->start(format)) {
            qWarning() << "Failed to start viewfinder with format:" << format;
        }
    }

    if (m_surface->isActive()) {
        m_surface->present(frame);
    }
}

void AalVideoRendererControl::onTextureCreated(GLuint textureID)
{
    // CLAUDE_DEBUG 2026-09-04: если у нас уже есть рабочая приватная
    // текстура и HAL-поток уже реально запущен через неё -- не даём
    // Qt Quick затереть её невалидным/нулевым значением (это ровно
    // симптом "Unable to get texture ID" на этом устройстве).
    if (m_privateGlReady && m_textureId && !textureID) {
        qWarning() << "CLAUDE_DEBUG: onTextureCreated(0) ignored, keeping private texture id=" << m_textureId;
        return;
    }

    m_textureId = textureID;
    CameraControl *cc = m_service->androidControl();
    if (cc) {
        android_camera_set_preview_texture(cc, m_textureId);
        if (m_textureId && m_previewStarted) {
            android_camera_start_preview(cc);
        }
    }
    m_service->updateCaptureReady();
}

void AalVideoRendererControl::onSnapshotTaken(QImage snapshotImage)
{
    m_preview = snapshotImage;
    Q_EMIT previewReady();
}

void AalVideoRendererControl::updateViewfinderFrameCB(void* context)
{
    Q_UNUSED(context);
    AalVideoRendererControl *self = AalCameraService::instance()->videoOutputControl();
    if (self->m_previewStarted) {
        QMetaObject::invokeMethod(self, "updateViewfinderFrame", Qt::QueuedConnection);
    }
}

void AalVideoRendererControl::onPreviewFrameCB(void* data, uint32_t data_size, void* context)
{
    Q_UNUSED(context);
    // CLAUDE_DEBUG 2026-09-05 (ФИКСЫ_ДЛЯ_В32.txt п.6.7): звонит HAL,
    // возможно с ЛЮБОГО потока (см. .h) -- сюда попадает реальный сырой
    // NV21-кадр. Сама конвертация NV21->RGB32 безопасна с любого потока
    // (локер внутри updateFromNV21()), но показ кадра (m_surface->present())
    // обязан идти на правильном потоке -- тот же QueuedConnection-приём,
    // что уже использовался для текстурного колбэка.
    AalVideoRendererControl *self = AalCameraService::instance()->videoOutputControl();
    if (!self || !self->m_previewStarted || !data) {
        return;
    }

    // CLAUDE_DEBUG 2026-09-05 (ФИКСЫ_ДЛЯ_В32.txt п.6.7, найдено decisive-
    // тестом): "stack corruption detected (-fstack-protector)" на первом
    // же реальном кадре случался ДАЖЕ с пустым телом колбэка -- виноват
    // оказался сам qWarning() внутри onPreviewFrameCB(): HAL зовёт этот
    // колбэк из потока с маленьким/нестандартным стеком (типично для
    // binder-колбэк-потоков), а сложное форматирование QDebug/QString
    // само по себе достаточно "дорого" по стеку, чтобы его переполнить.
    // ПРАВИЛО: НИКАКИХ qWarning()/qDebug() внутри onPreviewFrameCB() и
    // вызываемых из него функций (updateFromNV21()) -- только чистая
    // арифметика. Логировать можно ТОЛЬКО после того, как управление
    // вернулось на GUI-поток (внутри updateViewfinderFrame()).
    self->m_mapper->updateFromNV21(static_cast<const uint8_t*>(data), data_size);
    g_previewFrameCount.fetch_add(1, std::memory_order_relaxed);
    QMetaObject::invokeMethod(self, "updateViewfinderFrame", Qt::QueuedConnection);
}

const QImage &AalVideoRendererControl::preview() const
{
    return m_preview;
}

void AalVideoRendererControl::createPreview()
{
    if (!m_textureId || !m_service->androidControl())
        return;

    QSize vfSize = m_service->viewfinderControl()->currentSize();
    SharedSignal::instance()->setSnapshotSize(vfSize);
    SharedSignal::instance()->takeSnapshot(m_service->androidControl());
}
