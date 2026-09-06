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

#include "aalcamerafocuscontrol.h"
#include "aalcameracontrol.h"
#include "aalcameraservice.h"
#include "hal_call_timeout.h"

#include <QDebug>
#include <QTimer>

#include <algorithm>

/// The size of the full sensor is +/- of this one in x and y
const int focusFullSize = 1000;
/// The size of the focus area is  +/- of this one in x and y
const int focusRegionSize = 100;

AalCameraFocusControl::AalCameraFocusControl(AalCameraService *service, QObject *parent)
    : QCameraFocusControl(parent),
      m_service(service),
      m_focusMode(QCameraFocus::AutoFocus),
      m_focusPointMode(QCameraFocus::FocusPointAuto),
      m_focusPoint(0.0, 0.0),
      m_focusRunning(false)
{
    m_focusRegion.left = 0.0;
    m_focusRegion.right = 0.0;
    m_focusRegion.top = 0.0;
    m_focusRegion.bottom = 0.0;
    m_focusRegion.weight = -9.9;

    // CLAUDE_DEBUG 2026-09-04: см. .h -- страховка от зависшего on_msg_focus_cb
    m_focusWatchdog = new QTimer(this);
    m_focusWatchdog->setSingleShot(true);
    m_focusWatchdog->setInterval(2000);
    connect(m_focusWatchdog, &QTimer::timeout, this, &AalCameraFocusControl::onFocusWatchdogTimeout);
}

void AalCameraFocusControl::onFocusWatchdogTimeout()
{
    if (m_focusRunning) {
        qWarning() << "CLAUDE_DEBUG: focus watchdog fired -- HAL never called on_msg_focus_cb, forcing focusRunning=false";
        m_focusRunning = false;
        m_service->updateCaptureReady();
    }
}

QPointF AalCameraFocusControl::customFocusPoint() const
{
    return m_focusPoint;
}

QCameraFocus::FocusModes AalCameraFocusControl::focusMode() const
{
    return m_focusMode;
}

QCameraFocus::FocusPointMode AalCameraFocusControl::focusPointMode() const
{
    return m_focusPointMode;
}

QCameraFocusZoneList AalCameraFocusControl::focusZones() const
{
    return QCameraFocusZoneList();
}

bool AalCameraFocusControl::isFocusModeSupported(QCameraFocus::FocusModes mode) const
{
    if (mode == QCameraFocus::HyperfocalFocus)
        return false;

    return true;
}

bool AalCameraFocusControl::isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const
{
    if (mode == QCameraFocus::FocusPointFaceDetection)
        return false;

    return true;
}

void AalCameraFocusControl::setCustomFocusPoint(const QPointF &point)
{
    if (m_focusPoint == point)
        return;

    MeteringRegion meteringRegion;
    m_focusPoint = point;
    point2Region(m_focusPoint, m_focusRegion, meteringRegion);
    Q_EMIT customFocusPointChanged(m_focusPoint);

    if (m_service->androidControl()) {
        android_camera_set_metering_region(m_service->androidControl(), &meteringRegion);
        android_camera_set_focus_region(m_service->androidControl(), &m_focusRegion);
        startFocus();
    }
}

void AalCameraFocusControl::setFocusMode(QCameraFocus::FocusModes mode)
{
    if (m_focusMode == mode || !isFocusModeSupported(mode))
        return;

    m_focusRunning = false;
    m_service->updateCaptureReady();
    AutoFocusMode focusMode = qt2Android(mode);
    m_focusMode = mode;
    if (m_service->androidControl()) {
        android_camera_set_auto_focus_mode(m_service->androidControl(), focusMode);
    }

    Q_EMIT focusModeChanged(m_focusMode);
}

void AalCameraFocusControl::setFocusPointMode(QCameraFocus::FocusPointMode mode)
{
    if (m_focusPointMode == mode || !isFocusPointModeSupported(mode))
        return;

    m_focusPointMode = mode;
    Q_EMIT focusPointModeChanged(m_focusPointMode);
}

void AalCameraFocusControl::focusCB(void *context)
{
    Q_UNUSED(context);
    AalCameraFocusControl *self = AalCameraService::instance()->focusControl();
    self->m_focusRunning = false;
    QMetaObject::invokeMethod(self->m_focusWatchdog, "stop", Qt::QueuedConnection);
    QMetaObject::invokeMethod(AalCameraService::instance(),
                              "updateCaptureReady", Qt::QueuedConnection);
}

bool AalCameraFocusControl::isFocusBusy() const
{
    return m_focusRunning;
}

/*!
 * \brief AalCameraFocusControl::enablePhotoMode resets the last focus mode for photos
 */
void AalCameraFocusControl::enablePhotoMode()
{
    setFocusMode(QCameraFocus::AutoFocus);
}

/*!
 * \brief AalCameraFocusControl::enableVideoMode sets the focus mode to continuous for video
 */
void AalCameraFocusControl::enableVideoMode()
{
    setFocusMode(QCameraFocus::ContinuousFocus);
}

void AalCameraFocusControl::init(CameraControl *control, CameraControlListener *listener)
{
    listener->on_msg_focus_cb = &AalCameraFocusControl::focusCB;

    AutoFocusMode mode = qt2Android(m_focusMode);
    android_camera_set_auto_focus_mode(control, mode);
    m_focusRunning = false;
    m_focusWatchdog->stop();
    m_service->updateCaptureReady();
}

void AalCameraFocusControl::startFocus()
{
    if (!m_service->androidControl())
        return;

    m_focusRunning = true;
    m_focusWatchdog->start();
    m_service->updateCaptureReady();
    // 2026-08-30: confirmed live -- tap-to-focus during video recording
    // correlates with a multi-second audio dropout (silence recorded even
    // though the room was not silent), matching the same class of
    // synchronous-binder-call-into-a-possibly-busy-HAL issue guarded
    // everywhere else tonight. This call was the last one of its kind
    // still completely unguarded, called directly from the UI thread.
    {
        CameraControl *camera = m_service->androidControl();
        runHalCallWithTimeout([camera]() { android_camera_start_autofocus(camera); },
                              3000, "android_camera_start_autofocus()");
    }
}

AutoFocusMode AalCameraFocusControl::qt2Android(QCameraFocus::FocusModes mode)
{
    switch(mode) {
    case QCameraFocus::ManualFocus:
        return AUTO_FOCUS_MODE_OFF;
    case QCameraFocus::InfinityFocus:
        return AUTO_FOCUS_MODE_INFINITY;
    case QCameraFocus::ContinuousFocus:
        if (m_service->cameraControl()->captureMode() == QCamera::CaptureStillImage)
            return AUTO_FOCUS_MODE_CONTINUOUS_PICTURE;
        else
            return AUTO_FOCUS_MODE_CONTINUOUS_VIDEO;
    case QCameraFocus::MacroFocus:
        return AUTO_FOCUS_MODE_MACRO;
    case QCameraFocus::AutoFocus:
    case QCameraFocus::HyperfocalFocus:
    default:
        return AUTO_FOCUS_MODE_AUTO;
    }
}

QCameraFocus::FocusModes AalCameraFocusControl::android2Qt(AutoFocusMode mode)
{
    switch(mode) {
    case AUTO_FOCUS_MODE_OFF:
        return QCameraFocus::ManualFocus;
    case AUTO_FOCUS_MODE_INFINITY:
        return QCameraFocus::InfinityFocus;
    case AUTO_FOCUS_MODE_CONTINUOUS_PICTURE:
    case AUTO_FOCUS_MODE_CONTINUOUS_VIDEO:
        return QCameraFocus::ContinuousFocus;
    case AUTO_FOCUS_MODE_MACRO:
        return QCameraFocus::MacroFocus;
    case AUTO_FOCUS_MODE_AUTO:
    default:
        return QCameraFocus::AutoFocus;
    }
}

void AalCameraFocusControl::point2Region(const QPointF &point, FocusRegion& region, MeteringRegion& metering) const
{
    int centerX = (point.x() * (2* focusFullSize)) - focusFullSize;
    int maxCenterPosition = focusFullSize - focusRegionSize;
    centerX = std::max(std::min(centerX, maxCenterPosition), -maxCenterPosition);
    int centerY = (point.y() * (2 * focusFullSize)) - focusFullSize;
    centerY = std::max(std::min(centerY, maxCenterPosition), -maxCenterPosition);

    region.left = centerX - focusRegionSize;
    region.right = centerX + focusRegionSize;
    region.top = centerY - focusRegionSize;
    region.bottom = centerY + focusRegionSize;
    region.weight = 5;

    metering.left = region.left;
    metering.right = region.right;
    metering.top = region.top;
    metering.bottom = region.bottom;
    metering.weight = 5;
}
