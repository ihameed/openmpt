build dependencies:
 - visual studio 2012 professional+
 - vst 2.4 sdk (disable by defining NO_VST)
 - asio sdk
 - directx sdk
 - qt 5.1.1
 - qt visual studio add-in
 - boost :[

whee:
configure -platform win32-msvc2012 -opensource -confirm-license -mp -opengl desktop -c++11 -sse2 -debug-and-release -force-debug-info -ltcg -no-rtti -fully-process -nomake demos -nomake examples -no-openvg -nomake tests -no-dbus -no-directwrite -no-vcproj -no-audio-backend -no-qml-debug -no-angle

append "-Zi" to QMAKE_CFLAGS_RELEASE in mkspecs/win32-msvc2012/qmake.conf
append "/DEBUG /OPT:REF" to QMAKE_LFLAGS_RELEASE in mkspecs/win32-msvc2012/qmake.conf
