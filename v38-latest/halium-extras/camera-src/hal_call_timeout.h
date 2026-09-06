/*
 * 2026-08-30: shared helper for guarding hybris/camera_compatibility_layer
 * calls that make a synchronous binder call into cameraserver -- if the
 * underlying vendor camera HAL has crashed/wedged (a real, repeated
 * instability on this device, documented elsewhere in this project), such
 * a call can hang forever with no way to time it out from the caller's
 * side. Runs fn() on a worker thread and waits via a nested QEventLoop
 * (so the UI's own event pump keeps running, unlike a plain blocking
 * wait) bounded by timeoutMs. If fn() hasn't finished by then, this
 * returns anyway -- the worker keeps running in the background (leaked,
 * but harmless: it can only ever be stuck talking to a HAL that's already
 * dead) instead of freezing the app.
 *
 * Used from both AalMediaRecorderControl (record path) and
 * AalImageCaptureControl (photo path) -- kept here rather than duplicated
 * so both share one implementation.
 */
#ifndef HAL_CALL_TIMEOUT_H
#define HAL_CALL_TIMEOUT_H

#include <functional>

#include <QDebug>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrentRun>

inline void runHalCallWithTimeout(std::function<void()> fn, int timeoutMs, const char *what)
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

#endif // HAL_CALL_TIMEOUT_H
