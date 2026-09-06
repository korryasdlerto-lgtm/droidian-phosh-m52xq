/*
 * Copyright (C) 2013 Canonical, Ltd.
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

#ifndef AALVIDEORENDERERCONTROL_H
#define AALVIDEORENDERERCONTROL_H

#include <QImage>
#include <QVideoRendererControl>
#include <qgl.h>

class AalCameraService;
struct CameraControl;
struct CameraControlListener;
class AalTextureBufferMapper;

class AalVideoRendererControl : public QVideoRendererControl
{
    Q_OBJECT
public:
    AalVideoRendererControl(AalCameraService *service, QObject *parent = 0);
    ~AalVideoRendererControl();

    QAbstractVideoSurface *surface() const;
    void setSurface(QAbstractVideoSurface *surface);

    static void updateViewfinderFrameCB(void *context);
    // CLAUDE_DEBUG 2026-09-05 (ФИКСЫ_ДЛЯ_В32.txt п.6.7): callback-режим
    // Camera1 API -- HAL зовёт это с сырым NV21-буфером на КАЖДЫЙ кадр,
    // независимо от GL/текстурного пути (который так и не смог
    // отрендериться под software Qt Quick backend). Может звонить с
    // ЛЮБОГО потока HAL -- сам колбэк только конвертирует NV21->RGB32 в
    // буфер маппера, показ кадра идёт через updateViewfinderFrame() на
    // правильном потоке (Qt::QueuedConnection), как и раньше для
    // текстурного пути.
    static void onPreviewFrameCB(void *data, uint32_t data_size, void *context);

    // CLAUDE_DEBUG 2026-09-04: android_camera_start_preview() (реальный
    // запуск потока с сенсора) на штатном пути вызывается ТОЛЬКО если
    // m_textureId уже ненулевой -- а его должен создать qtvideo-node
    // (ShaderVideoNodePlugin) через сигнал textureCreated, который на этом
    // устройстве никогда не срабатывает ("Unable to get texture ID" --
    // EGLConfig недоступен в самом Qt Quick). Итог: поток с камеры не
    // стартует НИКОГДА, даже когда readyForCapture=true. Раньше это уже
    // обходили собственным приватным EGL-контекстом (проверено отдельным
    // тест-стендом egl-probe.c -- сырой EGL на устройстве полностью
    // рабочий, проблема только в связке Qt Quick/GLVND). Даём HAL
    // валидный texture id так же -- не для реального отображения на
    // экране (это отдельная, ещё не решённая проблема), а чтобы поток
    // кадров с сенсора реально пошёл (без этого фото в принципе невозможны).
    bool ensurePrivateGlContext();

    const QImage &preview() const;
    void createPreview();

    bool isPreviewStarted() const;

public Q_SLOTS:
    void init(CameraControl *control, CameraControlListener *listener);
    void startPreview();
    void stopPreview();

Q_SIGNALS:
    void surfaceChanged(QAbstractVideoSurface *surface);
    void previewReady();

private Q_SLOTS:
    void updateViewfinderFrame();
    void onTextureCreated(unsigned int textureID);
    void onSnapshotTaken(QImage snapshotImage);

private:
    QAbstractVideoSurface *m_surface;
    AalCameraService *m_service;
    AalTextureBufferMapper* m_mapper;

    bool m_viewFinderRunning;
    bool m_previewStarted;
    GLuint m_textureId;
    QImage m_preview;

    // CLAUDE_DEBUG: см. ensurePrivateGlContext() -- держим как члены, а не
    // статики, HAL требует вызова из того же потока, где создан контекст.
    bool m_privateGlReady;
    void *m_eglDpy;
    void *m_eglCtx;
    void *m_eglSurf;
};

#endif
