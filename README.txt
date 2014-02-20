build dependencies:
 - visual studio 2012 professional+
 - vst 2.4 sdk (disable by defining NO_VST)
 - asio sdk
 - directx sdk
 - qt 5.2.1
 - qt visual studio add-in
 - boost :[

whee:

configure ^
    -platform win32-msvc2012 ^
    -opensource -confirm-license ^
    -opengl desktop ^
    -mp -c++11 -sse2 -ltcg -no-rtti ^
    -debug-and-release -force-debug-info ^
    -fully-process ^
    -nomake examples ^
    -nomake tests ^
    -nomake tools ^
    -no-openvg -no-dbus -no-directwrite -no-vcproj ^
    -no-audio-backend -no-wmf-backend -no-qml-debug ^
    -no-icu -no-angle ^
    -skip qtactiveqt ^
    -skip qtandroidextras ^
    -skip qtconnectivity ^
    -skip qtgraphicaleffects ^
    -skip qtlocation ^
    -skip qtmacextras ^
    -skip qtmultimedia ^
    -skip qtquick1 ^
    -skip qtquickcontrols ^
    -skip qtsensors ^
    -skip qtserialport ^
    -skip qtwebkit ^
    -skip qtwebkit-examples ^
    -skip qtx11extras ^
    -skip qtxmlpatterns

in qtbase/mkspecs/win32-msvc2012/qmake.conf
QMAKE_CFLAGS_RELEASE    = $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO

in qtbase/mkspecs/win32-msvc2012/qmake.conf
QMAKE_LFLAGS_RELEASE    = $$QMAKE_LFLAGS_RELEASE_WITH_DEBUGINFO
