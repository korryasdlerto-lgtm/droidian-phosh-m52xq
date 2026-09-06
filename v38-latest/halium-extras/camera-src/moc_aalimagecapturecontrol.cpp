/****************************************************************************
** Meta object code from reading C++ file 'aalimagecapturecontrol.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "aalimagecapturecontrol.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'aalimagecapturecontrol.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AalImageCaptureControl_t {
    QByteArrayData data[11];
    char stringdata0[123];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AalImageCaptureControl_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AalImageCaptureControl_t qt_meta_stringdata_AalImageCaptureControl = {
    {
QT_MOC_LITERAL(0, 0, 22), // "AalImageCaptureControl"
QT_MOC_LITERAL(1, 23, 4), // "init"
QT_MOC_LITERAL(2, 28, 0), // ""
QT_MOC_LITERAL(3, 29, 14), // "CameraControl*"
QT_MOC_LITERAL(4, 44, 7), // "control"
QT_MOC_LITERAL(5, 52, 22), // "CameraControlListener*"
QT_MOC_LITERAL(6, 75, 8), // "listener"
QT_MOC_LITERAL(7, 84, 16), // "onImageFileSaved"
QT_MOC_LITERAL(8, 101, 7), // "shutter"
QT_MOC_LITERAL(9, 109, 8), // "saveJpeg"
QT_MOC_LITERAL(10, 118, 4) // "data"

    },
    "AalImageCaptureControl\0init\0\0"
    "CameraControl*\0control\0CameraControlListener*\0"
    "listener\0onImageFileSaved\0shutter\0"
    "saveJpeg\0data"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AalImageCaptureControl[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    2,   34,    2, 0x0a /* Public */,
       7,    0,   39,    2, 0x0a /* Public */,
       8,    0,   40,    2, 0x08 /* Private */,
       9,    1,   41,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 5,    4,    6,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QByteArray,   10,

       0        // eod
};

void AalImageCaptureControl::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AalImageCaptureControl *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->init((*reinterpret_cast< CameraControl*(*)>(_a[1])),(*reinterpret_cast< CameraControlListener*(*)>(_a[2]))); break;
        case 1: _t->onImageFileSaved(); break;
        case 2: _t->shutter(); break;
        case 3: _t->saveJpeg((*reinterpret_cast< const QByteArray(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject AalImageCaptureControl::staticMetaObject = { {
    &QCameraImageCaptureControl::staticMetaObject,
    qt_meta_stringdata_AalImageCaptureControl.data,
    qt_meta_data_AalImageCaptureControl,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *AalImageCaptureControl::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AalImageCaptureControl::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AalImageCaptureControl.stringdata0))
        return static_cast<void*>(this);
    return QCameraImageCaptureControl::qt_metacast(_clname);
}

int AalImageCaptureControl::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QCameraImageCaptureControl::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
