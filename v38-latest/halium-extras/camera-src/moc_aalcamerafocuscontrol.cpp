/****************************************************************************
** Meta object code from reading C++ file 'aalcamerafocuscontrol.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.12.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "aalcamerafocuscontrol.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'aalcamerafocuscontrol.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.12.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AalCameraFocusControl_t {
    QByteArrayData data[8];
    char stringdata0[94];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AalCameraFocusControl_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AalCameraFocusControl_t qt_meta_stringdata_AalCameraFocusControl = {
    {
QT_MOC_LITERAL(0, 0, 21), // "AalCameraFocusControl"
QT_MOC_LITERAL(1, 22, 4), // "init"
QT_MOC_LITERAL(2, 27, 0), // ""
QT_MOC_LITERAL(3, 28, 14), // "CameraControl*"
QT_MOC_LITERAL(4, 43, 7), // "control"
QT_MOC_LITERAL(5, 51, 22), // "CameraControlListener*"
QT_MOC_LITERAL(6, 74, 8), // "listener"
QT_MOC_LITERAL(7, 83, 10) // "startFocus"

    },
    "AalCameraFocusControl\0init\0\0CameraControl*\0"
    "control\0CameraControlListener*\0listener\0"
    "startFocus"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AalCameraFocusControl[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    2,   24,    2, 0x0a /* Public */,
       7,    0,   29,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3, 0x80000000 | 5,    4,    6,
    QMetaType::Void,

       0        // eod
};

void AalCameraFocusControl::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AalCameraFocusControl *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->init((*reinterpret_cast< CameraControl*(*)>(_a[1])),(*reinterpret_cast< CameraControlListener*(*)>(_a[2]))); break;
        case 1: _t->startFocus(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject AalCameraFocusControl::staticMetaObject = { {
    &QCameraFocusControl::staticMetaObject,
    qt_meta_stringdata_AalCameraFocusControl.data,
    qt_meta_data_AalCameraFocusControl,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *AalCameraFocusControl::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AalCameraFocusControl::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AalCameraFocusControl.stringdata0))
        return static_cast<void*>(this);
    return QCameraFocusControl::qt_metacast(_clname);
}

int AalCameraFocusControl::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QCameraFocusControl::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 2;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
