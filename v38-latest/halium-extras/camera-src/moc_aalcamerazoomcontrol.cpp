/****************************************************************************
** Meta object code from reading C++ file 'aalcamerazoomcontrol.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "aalcamerazoomcontrol.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'aalcamerazoomcontrol.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AalCameraZoomControl_t {
    QByteArrayData data[7];
    char stringdata0[82];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AalCameraZoomControl_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AalCameraZoomControl_t qt_meta_stringdata_AalCameraZoomControl = {
    {
QT_MOC_LITERAL(0, 0, 20), // "AalCameraZoomControl"
QT_MOC_LITERAL(1, 21, 4), // "init"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 14), // "CameraControl*"
QT_MOC_LITERAL(4, 42, 7), // "control"
QT_MOC_LITERAL(5, 50, 22), // "CameraControlListener*"
QT_MOC_LITERAL(6, 73, 8) // "listener"

    },
    "AalCameraZoomControl\0init\0\0CameraControl*\0"
    "control\0CameraControlListener*\0listener"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AalCameraZoomControl[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    2,   19,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 5,    4,    6,

       0        // eod
};

void AalCameraZoomControl::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AalCameraZoomControl *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->init((*reinterpret_cast< CameraControl*(*)>(_a[1])),(*reinterpret_cast< CameraControlListener*(*)>(_a[2]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject AalCameraZoomControl::staticMetaObject = { {
    &QCameraZoomControl::staticMetaObject,
    qt_meta_stringdata_AalCameraZoomControl.data,
    qt_meta_data_AalCameraZoomControl,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *AalCameraZoomControl::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AalCameraZoomControl::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AalCameraZoomControl.stringdata0))
        return static_cast<void*>(this);
    return QCameraZoomControl::qt_metacast(_clname);
}

int AalCameraZoomControl::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QCameraZoomControl::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 1;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
