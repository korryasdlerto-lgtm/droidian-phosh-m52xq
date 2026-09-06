/****************************************************************************
** Meta object code from reading C++ file 'aalmediarecordercontrol.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "aalmediarecordercontrol.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'aalmediarecordercontrol.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AalMediaRecorderControl_t {
    QByteArrayData data[14];
    char stringdata0[188];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AalMediaRecorderControl_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AalMediaRecorderControl_t qt_meta_stringdata_AalMediaRecorderControl = {
    {
QT_MOC_LITERAL(0, 0, 23), // "AalMediaRecorderControl"
QT_MOC_LITERAL(1, 24, 25), // "audioCaptureThreadStarted"
QT_MOC_LITERAL(2, 50, 0), // ""
QT_MOC_LITERAL(3, 51, 8), // "setMuted"
QT_MOC_LITERAL(4, 60, 5), // "muted"
QT_MOC_LITERAL(5, 66, 8), // "setState"
QT_MOC_LITERAL(6, 75, 21), // "QMediaRecorder::State"
QT_MOC_LITERAL(7, 97, 5), // "state"
QT_MOC_LITERAL(8, 103, 9), // "setVolume"
QT_MOC_LITERAL(9, 113, 4), // "gain"
QT_MOC_LITERAL(10, 118, 23), // "startAudioCaptureThread"
QT_MOC_LITERAL(11, 142, 14), // "updateDuration"
QT_MOC_LITERAL(12, 157, 11), // "handleError"
QT_MOC_LITERAL(13, 169, 18) // "deleteAudioCapture"

    },
    "AalMediaRecorderControl\0"
    "audioCaptureThreadStarted\0\0setMuted\0"
    "muted\0setState\0QMediaRecorder::State\0"
    "state\0setVolume\0gain\0startAudioCaptureThread\0"
    "updateDuration\0handleError\0"
    "deleteAudioCapture"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AalMediaRecorderControl[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   54,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    1,   55,    2, 0x0a /* Public */,
       5,    1,   58,    2, 0x0a /* Public */,
       8,    1,   61,    2, 0x0a /* Public */,
      10,    0,   64,    2, 0x0a /* Public */,
      11,    0,   65,    2, 0x08 /* Private */,
      12,    0,   66,    2, 0x08 /* Private */,
      13,    0,   67,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::Bool,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, QMetaType::QReal,    9,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void AalMediaRecorderControl::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AalMediaRecorderControl *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->audioCaptureThreadStarted(); break;
        case 1: _t->setMuted((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 2: _t->setState((*reinterpret_cast< QMediaRecorder::State(*)>(_a[1]))); break;
        case 3: _t->setVolume((*reinterpret_cast< qreal(*)>(_a[1]))); break;
        case 4: _t->startAudioCaptureThread(); break;
        case 5: _t->updateDuration(); break;
        case 6: _t->handleError(); break;
        case 7: _t->deleteAudioCapture(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QMediaRecorder::State >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AalMediaRecorderControl::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AalMediaRecorderControl::audioCaptureThreadStarted)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject AalMediaRecorderControl::staticMetaObject = { {
    &QMediaRecorderControl::staticMetaObject,
    qt_meta_stringdata_AalMediaRecorderControl.data,
    qt_meta_data_AalMediaRecorderControl,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *AalMediaRecorderControl::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AalMediaRecorderControl::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AalMediaRecorderControl.stringdata0))
        return static_cast<void*>(this);
    return QMediaRecorderControl::qt_metacast(_clname);
}

int AalMediaRecorderControl::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMediaRecorderControl::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void AalMediaRecorderControl::audioCaptureThreadStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
