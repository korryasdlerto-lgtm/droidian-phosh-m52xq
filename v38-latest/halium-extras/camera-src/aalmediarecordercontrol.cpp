/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
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

#include "aalmediarecordercontrol.h"
#include "aalcameraservice.h"
#include "aalmetadatawritercontrol.h"
#include "aalvideoencodersettingscontrol.h"
#include "aalviewfindersettingscontrol.h"
#include "audiocapture.h"
#include "storagemanager.h"
#include "rotationhandler.h"

#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrentRun>

#include <hybris/camera/camera_compatibility_layer.h>
#include <hybris/camera/camera_compatibility_layer_capabilities.h>
#include <hybris/media/media_recorder_layer.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

const int AalMediaRecorderControl::RECORDER_GENERAL_ERROR;
const int AalMediaRecorderControl::RECORDER_NOT_AVAILABLE_ERROR;
const int AalMediaRecorderControl::RECORDER_INITIALIZATION_ERROR;

const int AalMediaRecorderControl::DURATION_UPDATE_INTERVAL;

const QLatin1String AalMediaRecorderControl::PARAM_AUDIO_BITRATE = QLatin1String("audio-param-encoding-bitrate");
const QLatin1String AalMediaRecorderControl::PARAM_AUDIO_CHANNELS = QLatin1String("audio-param-number-of-channels");
const QLatin1String AalMediaRecorderControl::PARAM_AUTIO_SAMPLING = QLatin1String("audio-param-sampling-rate");
const QLatin1String AalMediaRecorderControl::PARAM_LATITUDE = QLatin1String("param-geotag-latitude");
const QLatin1String AalMediaRecorderControl::PARAM_LONGITUDE = QLatin1String("param-geotag-longitude");
const QLatin1String AalMediaRecorderControl::PARAM_ORIENTATION = QLatin1String("video-param-rotation-angle-degrees");
const QLatin1String AalMediaRecorderControl::PARAM_VIDEO_BITRATE = QLatin1String("video-param-encoding-bitrate");
/*!
 * \brief AalMediaRecorderControl::AalMediaRecorderControl
 * \param service
 * \param parent
 */
AalMediaRecorderControl::AalMediaRecorderControl(AalCameraService *service, QObject *parent)
   : QMediaRecorderControl(parent),
    m_service(service),
    m_mediaRecorder(0),
    m_audioCapture(0),
    m_outfd(-1),
    m_duration(0),
    m_currentState(QMediaRecorder::StoppedState),
    m_currentStatus(QMediaRecorder::UnloadedStatus),
    m_recordingTimer(0),
    m_audioCaptureAvailable(false)
{
}

/*!
 * \brief AalMediaRecorderControl::~AalMediaRecorderControl
 */
AalMediaRecorderControl::~AalMediaRecorderControl()
{
    delete m_recordingTimer;
    if (m_outfd != -1)
    {
        int err = close(m_outfd);
        if (err < 0)
            qWarning() << "Failed to close recording output file descriptor (errno: "
                << errno << ")";
    }
    deleteRecorder();
    m_audioCaptureThread.quit();
    m_audioCaptureThread.wait();
}

/*!
 * \reimp
 */
void AalMediaRecorderControl::applySettings()
{
    qDebug() << Q_FUNC_INFO << " is not used";
}

/*!
 * \reimp
 */
qint64 AalMediaRecorderControl::duration() const
{
    return m_duration;
}

/*!
 * \reimp
 */
bool AalMediaRecorderControl::isMuted() const
{
    qDebug() << Q_FUNC_INFO << " is not used";
    return false;
}

/*!
 * \reimp
 */
QUrl AalMediaRecorderControl::outputLocation() const
{
    return m_outputLocation;
}

/*!
 * \reimp
 */
bool AalMediaRecorderControl::setOutputLocation(const QUrl &location)
{
    if ( m_outputLocation == location)
        return true;

    m_outputLocation = location;
    return true;
}

/*!
 * \reimp
 */
QMediaRecorder::State AalMediaRecorderControl::state() const
{
    return m_currentState;
}

/*!
 * \reimp
 */
QMediaRecorder::Status AalMediaRecorderControl::status() const
{
    return m_currentStatus;
}

/*!
 * \reimp
 */
qreal AalMediaRecorderControl::volume() const
{
    qDebug() << Q_FUNC_INFO << " is not used";
    return 1.0;
}

/*!
 * \brief Starts the main microphone reader/writer loop in AudioCapture (run)
 */
void AalMediaRecorderControl::startAudioCaptureThread()
{
    qDebug() << "Starting microphone reader/writer thread";
    // Start the microphone read/write thread
    m_audioCaptureThread.start();
    Q_EMIT audioCaptureThreadStarted();
}

/*!
 * \brief AalMediaRecorderControl::init makes sure the mediarecorder is
 * initialized
 */
bool AalMediaRecorderControl::initRecorder()
{
    if (m_mediaRecorder == 0) {
        m_mediaRecorder = android_media_new_recorder();
        if (m_mediaRecorder == 0) {
            qWarning() << "Unable to create new media recorder";
            Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "Unable to create new media recorder");
            return false;
        }

        int audioInitError = initAudioCapture();
        if (audioInitError == 0) {
            m_audioCaptureAvailable = true;
        } else {
            m_audioCaptureAvailable = false;
            if (audioInitError == AudioCapture::AUDIO_CAPTURE_TIMEOUT_ERROR) {
                deleteRecorder();
                return false;
            }
        }

        android_recorder_set_error_cb(m_mediaRecorder, &AalMediaRecorderControl::errorCB, this);
        // 2026-08-30: same binder-into-cameraserver hang class as
        // android_recorder_release()/android_camera_stop_recording() above
        // -- confirmed live: a recording attempt right after the HAL had
        // already wedged (detected via the release() timeout) hung again
        // here, on the very first camera call of the NEXT initRecorder(),
        // with zero further log output. Same timeout guard.
        {
            fprintf(stderr, "CLAUDE_DEBUG: initRecorder() calling android_camera_unlock()\n");
            CameraControl *camera = m_service->androidControl();
            runWithTimeout([camera]() { android_camera_unlock(camera); },
                           3000, "android_camera_unlock()");
            fprintf(stderr, "CLAUDE_DEBUG: initRecorder() android_camera_unlock() done\n");
        }
    }

    return true;
}

/*!
 * \brief Runs fn() on a worker thread with a bounded wait -- see header for why.
 */
void AalMediaRecorderControl::runWithTimeout(std::function<void()> fn, int timeoutMs, const char *what)
{
    QFuture<void> future = QtConcurrent::run(fn);
    QFutureWatcher<void> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(future);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    if (!future.isFinished()) {
        qWarning() << what << "did not return within" << timeoutMs
                   << "ms -- camera HAL likely wedged, proceeding anyway";
    }
}

/*!
 * \brief AalMediaRecorderControl::deleteRecorder releases all resources and
 * deletes the MediaRecorder
 */
void AalMediaRecorderControl::deleteRecorder()
{
    fprintf(stderr, "CLAUDE_DEBUG: deleteRecorder() calling deleteAudioCapture()\n");
    deleteAudioCapture();
    fprintf(stderr, "CLAUDE_DEBUG: deleteAudioCapture() returned\n");

    if (m_mediaRecorder == 0)
        return;

    // 2026-08-30: confirmed live via CLAUDE_DEBUG tracing -- this is the
    // actual hang point when the vendor camera HAL is wedged (all other
    // teardown calls -- stop_recording, recorder_stop, the whole audio
    // capture shutdown -- completed in well under a second; the log went
    // silent right after this call started and never printed its return).
    // android_recorder_release() makes a binder call all the way down
    // into cameraserver/the HAL, same class of hang as
    // android_camera_stop_recording() above -- same timeout guard.
    {
        MediaRecorderWrapper *recorder = m_mediaRecorder;
        runWithTimeout([recorder]() { android_recorder_release(recorder); },
                       3000, "android_recorder_release()");
    }
    m_mediaRecorder = 0;
    {
        CameraControl *camera = m_service->androidControl();
        runWithTimeout([camera]() { android_camera_lock(camera); },
                       3000, "android_camera_lock()");
    }
    fprintf(stderr, "CLAUDE_DEBUG: deleteRecorder() done\n");
    setStatus(QMediaRecorder::UnloadedStatus);
}

int AalMediaRecorderControl::initAudioCapture()
{
    // setting up audio recording; m_audioCapture is executed within the m_workerThread affinity
    m_audioCapture = new AudioCapture(m_mediaRecorder);
    int audioInitError = m_audioCapture->setupMicrophoneStream();
    if (audioInitError != 0)
    {
        qWarning() << "Failed to setup PulseAudio microphone recording stream";
        delete m_audioCapture;
        m_audioCapture = 0;
    } else {
        m_audioCapture->moveToThread(&m_audioCaptureThread);

        // startWorkerThread signal comes from an Android layer callback that resides down in
        // the AudioRecordHybris class
        connect(this, SIGNAL(audioCaptureThreadStarted()), m_audioCapture, SLOT(run()));

        // Call recorderReadAudioCallback when the reader side of the named pipe has been setup
        m_audioCapture->init(&AalMediaRecorderControl::recorderReadAudioCallback, this);
    }
    return audioInitError;
}

void AalMediaRecorderControl::deleteAudioCapture()
{
    if (m_audioCapture == 0)
        return;

    fprintf(stderr, "CLAUDE_DEBUG: deleteAudioCapture() calling stopCapture()\n");
    m_audioCapture->stopCapture();
    fprintf(stderr, "CLAUDE_DEBUG: deleteAudioCapture() calling thread.quit()/wait()\n");
    m_audioCaptureThread.quit();
    m_audioCaptureThread.wait();
    fprintf(stderr, "CLAUDE_DEBUG: deleteAudioCapture() thread.wait() returned\n");

    delete m_audioCapture;
    m_audioCapture = 0;
    m_audioCaptureAvailable = false;
}

/*!
 * \brief AalMediaRecorderControl::errorCB handles errors from the android layer
 * \param context
 */
void AalMediaRecorderControl::errorCB(void *context)
{
    Q_UNUSED(context);
    QMetaObject::invokeMethod(AalCameraService::instance()->mediaRecorderControl(),
                              "handleError", Qt::QueuedConnection);
}

MediaRecorderWrapper* AalMediaRecorderControl::mediaRecorder() const
{
    return m_mediaRecorder;
}

AudioCapture *AalMediaRecorderControl::audioCapture() const
{
    return m_audioCapture;
}

/*!
 * \reimp
 */
void AalMediaRecorderControl::setMuted(bool muted)
{
    Q_UNUSED(muted);
    qDebug() << Q_FUNC_INFO << " is not used";
}

/*!
 * \reimp
 */
void AalMediaRecorderControl::setState(QMediaRecorder::State state)
{
    if (m_currentState == state)
        return;

    switch (state) {
    case QMediaRecorder::RecordingState: {
        startRecording();
        break;
    }
    case QMediaRecorder::StoppedState: {
        stopRecording();
        break;
    }
    case QMediaRecorder::PausedState: {
        qDebug() << Q_FUNC_INFO << " pause not used for video recording.";
        break;
    }
    }
}

/*!
 * \reimp
 */
void AalMediaRecorderControl::setVolume(qreal gain)
{
    Q_UNUSED(gain);
    qDebug() << Q_FUNC_INFO << " is not used";
}

void AalMediaRecorderControl::updateDuration()
{
    m_duration += DURATION_UPDATE_INTERVAL;
    Q_EMIT durationChanged(m_duration);
}

/*!
 * \brief AalMediaRecorderControl::handleError emits errors from android layer
 */
void AalMediaRecorderControl::handleError()
{
    Q_EMIT error(RECORDER_GENERAL_ERROR, "Error on recording video");
}

/*!
 * \brief AalMediaRecorderControl::setStatus
 * \param status
 */
void AalMediaRecorderControl::setStatus(QMediaRecorder::Status status)
{
    if (m_currentStatus == status)
        return;

    m_currentStatus = status;
    Q_EMIT statusChanged(m_currentStatus);
}

/*!
 * \brief AalMediaRecorderControl::startRecording starts a video record.
 * FIXME add support for recording audio only
 */
int AalMediaRecorderControl::startRecording()
{
    if (m_service->androidControl() == 0) {
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "No camera connection");
        return RECORDER_INITIALIZATION_ERROR;
    }

    if (m_currentStatus != QMediaRecorder::UnloadedStatus) {
        qWarning() << "Can't start a recording while another one is in progess";
        return RECORDER_NOT_AVAILABLE_ERROR;
    }

    setStatus(QMediaRecorder::LoadingStatus);

    m_duration = 0;
    Q_EMIT durationChanged(m_duration);

    if (!initRecorder()) {
        setStatus(QMediaRecorder::UnloadedStatus);
        return RECORDER_NOT_AVAILABLE_ERROR;
    }

    QVideoEncoderSettings videoSettings = m_service->videoEncoderControl()->videoSettings();

    int ret;
    fprintf(stderr, "CLAUDE_DEBUG: startRecording() calling android_recorder_setCamera()\n");
    ret = android_recorder_setCamera(m_mediaRecorder, m_service->androidControl());
    fprintf(stderr, "CLAUDE_DEBUG: android_recorder_setCamera() returned %d\n", ret);
    if (ret < 0) {
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setCamera() failed\n");
        return RECORDER_INITIALIZATION_ERROR;
    }
    // state initial / idle
    if (m_audioCaptureAvailable) {
        fprintf(stderr, "CLAUDE_DEBUG: startRecording() calling android_recorder_setAudioSource()\n");
        ret = android_recorder_setAudioSource(m_mediaRecorder, ANDROID_AUDIO_SOURCE_CAMCORDER);
        fprintf(stderr, "CLAUDE_DEBUG: android_recorder_setAudioSource() returned %d\n", ret);
        if (ret < 0) {
            deleteRecorder();
            Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setAudioSource() failed");
            return RECORDER_INITIALIZATION_ERROR;
        }

    }
    // 2026-08-28: tried switching this to ANDROID_VIDEO_SOURCE_GRALLOC_BUFFER
    // + Camera::setVideoBufferMode()/setVideoTarget() (bypassing the legacy,
    // never-finished-since-2014 CameraSource/ICameraRecordingProxy path) --
    // real progress (encoder/recorder threads actually get created now) but
    // it hits a NEW failure mode: prepare() hangs indefinitely (binder
    // transaction never delivered to any camera_service thread, confirmed
    // via core dump -- all 5 threads idle in libc syscall wait). Reverted to
    // the known-quantity CAMERA source (fails fast after ~3s instead of
    // hanging forever) until that binder-level stall gets its own
    // ftrace-based investigation. See ФИКСЫ_ДЛЯ_В48.txt for the full story.
    fprintf(stderr, "CLAUDE_DEBUG: startRecording() calling android_recorder_setVideoSource()\n");
    ret = android_recorder_setVideoSource(m_mediaRecorder, ANDROID_VIDEO_SOURCE_GRALLOC_BUFFER);
    fprintf(stderr, "CLAUDE_DEBUG: android_recorder_setVideoSource() returned %d\n", ret);
    if (ret < 0) {
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setVideoSource() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }
    // state initialized
    fprintf(stderr, "CLAUDE_DEBUG: startRecording() calling android_recorder_setOutputFormat()\n");
    ret = android_recorder_setOutputFormat(m_mediaRecorder, ANDROID_OUTPUT_FORMAT_MPEG_4);
    fprintf(stderr, "CLAUDE_DEBUG: android_recorder_setOutputFormat() returned %d\n", ret);
    if (ret < 0) {
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setOutputFormat() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }
    // state DataSourceConfigured
    if (m_audioCaptureAvailable) {
        fprintf(stderr, "CLAUDE_DEBUG: startRecording() calling android_recorder_setAudioEncoder()\n");
        ret = android_recorder_setAudioEncoder(m_mediaRecorder, ANDROID_AUDIO_ENCODER_AAC);
        fprintf(stderr, "CLAUDE_DEBUG: android_recorder_setAudioEncoder() returned %d\n", ret);
        if (ret < 0) {
            deleteRecorder();
            Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setAudioEncoder() failed");
            return RECORDER_INITIALIZATION_ERROR;
        }
    }
    // FIXME set codec from settings
    fprintf(stderr, "CLAUDE_DEBUG: startRecording() calling android_recorder_setVideoEncoder()\n");
    ret = android_recorder_setVideoEncoder(m_mediaRecorder, ANDROID_VIDEO_ENCODER_H264);
    fprintf(stderr, "CLAUDE_DEBUG: android_recorder_setVideoEncoder() returned %d\n", ret);
    if (ret < 0) {
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setVideoEncoder() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }

    QString fileName = m_outputLocation.path();
    QFileInfo fileInfo = QFileInfo(fileName);
    if (fileName.isEmpty()) {
        fileName = m_service->storageManager()->nextVideoFileName();
    } else if (fileInfo.isDir()) {
        fileName = m_service->storageManager()->nextVideoFileName(fileName);
    }
    Q_EMIT actualLocationChanged(QUrl(fileName));

    m_outfd = open(fileName.toLocal8Bit().data(), O_WRONLY | O_CREAT,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (m_outfd < 0) {
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "Could not open file for video recording");
        return RECORDER_INITIALIZATION_ERROR;
    }
    ret = android_recorder_setOutputFile(m_mediaRecorder, m_outfd);
    if (ret < 0) {
        close(m_outfd);
        m_outfd = -1;
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setOutputFile() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }

    QSize resolution = videoSettings.resolution();
    // 2026-08-30: root cause of the earlier "Graphic buf 1920x1080 doesn't
    // match configured 3840x2160" encoder rejection -- android_recorder_setVideoSize()
    // only configures the ENCODER's target size on the MediaRecorder object.
    // It does NOT tell the camera HAL what size its own recording stream
    // should be: that is a separate camera parameter ("video-size"), set via
    // android_camera_set_video_size() on the CameraControl, which qtubuntu-camera
    // was never calling. Without it the camera's video-record stream stayed
    // at its default (preview) size while the encoder was told a different,
    // larger size -- guaranteed buffer-size mismatch at every resolution
    // other than whatever the default happened to match. Set both sides
    // together so the camera's stream and the encoder's expectation agree.
    fprintf(stderr, "CLAUDE_DEBUG: startRecording() calling android_camera_set_video_size()\n");
    android_camera_set_video_size(m_service->androidControl(), resolution.width(), resolution.height());
    fprintf(stderr, "CLAUDE_DEBUG: android_camera_set_video_size() done, calling android_recorder_setVideoSize()\n");
    ret = android_recorder_setVideoSize(m_mediaRecorder, resolution.width(), resolution.height());
    fprintf(stderr, "CLAUDE_DEBUG: android_recorder_setVideoSize() returned %d\n", ret);
    if (ret < 0) {
        close(m_outfd);
        m_outfd = -1;
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setVideoSize() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }
    ret = android_recorder_setVideoFrameRate(m_mediaRecorder, videoSettings.frameRate());
    if (ret < 0) {
        close(m_outfd);
        m_outfd = -1;
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setVideoFrameRate() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }

    setParameter(PARAM_VIDEO_BITRATE, videoSettings.bitRate());
    // FIXME get data from a new AalAudioEncoderSettingsControl
    setParameter(PARAM_AUDIO_BITRATE, 48000);
    setParameter(PARAM_AUDIO_CHANNELS, 2);
    setParameter(PARAM_AUTIO_SAMPLING, 96000);

    int rotation = m_service->rotationHandler()->calculateRotation();
    setParameter(PARAM_ORIENTATION, rotation);

    if (m_service->metadataWriterControl()) {
        // FIXME: what metadata can be supported?
        m_service->metadataWriterControl()->clearAllMetaData();
    }

    // 2026-08-30: confirmed live (twice) via CLAUDE_DEBUG tracing -- this is
    // where a fresh recording attempt hangs once the vendor HAL is already
    // wedged from a previous session (matches the older comment above about
    // prepare() hanging indefinitely on a binder transaction that's never
    // delivered). Same runWithTimeout() guard as the teardown path. Unlike
    // the void-returning calls above, prepare()'s return value is actually
    // checked below, so the result is captured through a shared_ptr (not a
    // reference to a local) -- if the timeout fires, this function may
    // already have returned by the time the background call (still stuck
    // on the dead HAL) finally completes and writes into it; a shared_ptr
    // keeps that write safe (the int outlives us) instead of writing into
    // a freed stack slot. Times out => treated as failure (ret stays -1,
    // hits the existing deleteRecorder()+error() path below) rather than
    // optimistically continuing into more calls on a HAL we know is dead.
    fprintf(stderr, "CLAUDE_DEBUG: startRecording() calling android_recorder_prepare()\n");
    {
        auto resultPtr = std::make_shared<int>(-1);
        MediaRecorderWrapper *recorder = m_mediaRecorder;
        runWithTimeout([recorder, resultPtr]() { *resultPtr = android_recorder_prepare(recorder); },
                       3000, "android_recorder_prepare()");
        ret = *resultPtr;
    }
    fprintf(stderr, "CLAUDE_DEBUG: android_recorder_prepare() returned %d\n", ret);
    if (ret < 0) {
        close(m_outfd);
        m_outfd = -1;
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_prepare() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }

    // 2026-08-28/29: the encoder's GraphicBufferProducer only exists once
    // prepare() has run (same contract as real AOSP's MediaRecorder.
    // getSurface()) -- must call this here, after prepare() and before
    // start(), to actually hand the camera something to write frames into.
    ret = android_recorder_setCameraAsVideoSource(m_mediaRecorder, m_service->androidControl());
    if (ret < 0) {
        close(m_outfd);
        m_outfd = -1;
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_setCameraAsVideoSource() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }

    setStatus(QMediaRecorder::LoadedStatus);
    setStatus(QMediaRecorder::StartingStatus);

    // state prepared
    ret = android_recorder_start(m_mediaRecorder);
    if (ret < 0) {
        close(m_outfd);
        m_outfd = -1;
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_start() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }

    // 2026-08-29: only NOW tell the camera HAL to actually start pushing
    // frames -- the encoder is running and draining the surface's buffer
    // queue at this point. Doing this before android_recorder_start()
    // deadlocked the whole HAL capture pipeline (confirmed live: viewfinder
    // froze solid, recording timer kept counting, video track stayed
    // completely empty even after a 20s take).
    ret = android_recorder_startCameraRecording(m_service->androidControl());
    if (ret < 0) {
        android_recorder_stop(m_mediaRecorder);
        close(m_outfd);
        m_outfd = -1;
        deleteRecorder();
        Q_EMIT error(RECORDER_INITIALIZATION_ERROR, "android_recorder_startCameraRecording() failed");
        return RECORDER_INITIALIZATION_ERROR;
    }

    m_currentState = QMediaRecorder::RecordingState;
    Q_EMIT stateChanged(m_currentState);

    setStatus(QMediaRecorder::RecordingStatus);

    if (m_recordingTimer == 0) {
        m_recordingTimer = new QTimer(this);
        m_recordingTimer->setInterval(DURATION_UPDATE_INTERVAL);
        m_recordingTimer->setSingleShot(false);
        QObject::connect(m_recordingTimer, SIGNAL(timeout()),
                         this, SLOT(updateDuration()));
    }
    m_recordingTimer->start();

    return 0;
}

/*!
 * \brief AalMediaRecorderControl::stopRecording
 */
void AalMediaRecorderControl::stopRecording()
{
    qDebug() << __PRETTY_FUNCTION__;
    if (m_mediaRecorder == 0) {
        qWarning() << "Can't stop recording properly, m_mediaRecorder is NULL";
        return;
    }

    if (m_currentStatus != QMediaRecorder::RecordingStatus) {
        qWarning() << "Can't stop a recording that has not started";
        return;
    }

    setStatus(QMediaRecorder::FinalizingStatus);
    m_recordingTimer->stop();

    // 2026-08-29: must stop the camera HAL from pushing frames to the
    // recording target surface before tearing the recorder down -- mirrors
    // real AOSP CameraSource::stop(), which calls camera->stopRecording()
    // as its very first action.
    //
    // 2026-08-30: android_camera_stop_recording() makes a synchronous
    // binder call into cameraserver -- if the underlying vendor camera
    // HAL has crashed/wedged (confirmed live: cameraserver's own binder
    // thread stuck forever waiting on a dead HAL after a provider crash
    // mid-recording), this call never returns and freezes the whole app,
    // since stopRecording() runs on the UI thread. Same timeout pattern
    // AOSP's own CameraX (Camera2CameraImpl) uses around camera
    // close/release for exactly this class of vendor-HAL hang.
    {
        CameraControl *camera = m_service->androidControl();
        runWithTimeout([camera]() { android_camera_stop_recording(camera); },
                       3000, "android_camera_stop_recording()");
    }

    fprintf(stderr, "CLAUDE_DEBUG: stopRecording() calling android_recorder_stop()\n");
    int result = android_recorder_stop(m_mediaRecorder);
    fprintf(stderr, "CLAUDE_DEBUG: android_recorder_stop() returned %d\n", result);
    if (result < 0) {
        Q_EMIT error(RECORDER_GENERAL_ERROR, "Cannot stop video recording");
        return;
    }

    // Stop microphone reader/writer loop
    // NOTE: This must come after the android_recorder_stop call, otherwise the
    // RecordThread instance will block the MPEG4Writer pthread_join when trying to
    // cleanly stop recording.
    if (m_audioCapture != 0) {
        m_audioCapture->stopCapture();
    }

    android_recorder_reset(m_mediaRecorder);

    int err = close(m_outfd);
    if (err < 0)
        qWarning() << "Failed to close recording output file descriptor (errno: "
            << errno << ")";
    m_outfd = -1;

    m_currentState = QMediaRecorder::StoppedState;
    Q_EMIT stateChanged(m_currentState);

    deleteRecorder();
}

/*!
 * \brief AalMediaRecorderControl::setParameter convenient function to set parameters
 * \param parameter Name of the parameter
 * \param value value to set
 */
void AalMediaRecorderControl::setParameter(const QString &parameter, int value)
{
    Q_ASSERT(m_mediaRecorder);
    QString param =  parameter + QChar('=') + QString::number(value);
    android_recorder_setParameters(m_mediaRecorder, param.toLocal8Bit().data());
}

void AalMediaRecorderControl::recorderReadAudioCallback(void *context)
{
    AalMediaRecorderControl *thiz = static_cast<AalMediaRecorderControl*>(context);
    if (thiz != NULL) {
        thiz->startAudioCaptureThread();
    }
}
