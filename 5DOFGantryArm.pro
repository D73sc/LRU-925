# DEFINES -= UNICODE
# DEFINES += UMBCS
QMAKE_CXXFLAGS -= -Zc:strictStrings

QT += core gui widgets charts opengl serialport

QT += network sql concurrent svg

QT += core gui multimedia multimediawidgets #多媒体模块

QT += sql  #MySQL数据库

TARGET = 5DOFGantryArm
TEMPLATE = app

# CONFIG += c++17 unicode
CONFIG += c++17 unicode

QMAKE_CXXFLAGS += /utf-8

QMAKE_CXXFLAGS += /wd4828

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
# CONFIG += c++11





SOURCES += \
    CrashHandler.cpp \
    CrashHandlerDbg.cpp \
    DatabaseManager.cpp \
    STLModel/coverplatewidget.cpp \
    STLModel/newform.cpp \
    STLModel/rrglwidget.cpp \
    STLModel/stlfileloader.cpp \
    SafetyMonitor.cpp \
    SerialSensor.cpp \
    SharedMemory/sharedmemorymanager.cpp \
    StepperMotor.cpp \
    SCStatusTcp.cpp \
    TCPSensor.cpp \
    mainwindow.cpp \
    motorcontroller.cpp \
    srisensorworker.cpp \
    uvc_camera.cpp \
    vision/code/lenslic/LensAccess.c \
    vision/code/lenslic/LensConnect_Controller.c \
    vision/code/lenslic/LensCtrl.c \
    vision/code/lenslic/LensInfo.c \
    vision/code/lenslic/LensSetup.c \
    vision/code/lenslic/UsbCtrl.c \
    vision_source/CDeviceProcess.cpp \
    vision_source/CGXBitmap.cpp \
    vision_source/arucodetector.cpp \
    vision_source/dahengTwoCams_qt_vs.cpp \
    vision_source/dmdetected.cpp \
    vision_source/guangzilasersensor.cpp \
    vision_source/imageviewer.cpp \
    vision_source/lightsourcecontroller.cpp \
    vision_source/udpcommunication.cpp \
    zmotioncontrol.cpp \
    agvmonitor.cpp \
    levelmonitor.cpp \
    liftmonitor.cpp \
    coverplatemonitor.cpp \
    pinmonitor.cpp \
    SCVehicle.cpp \
    LevelingSystem.cpp \
    MainLift.cpp \
    CoverPlate.cpp \
    Lift.cpp \
    Pin.cpp \
    TaskMgr.cpp \
    loghandler.cpp \
    logmgr.cpp \
    LRU.cpp \
    lrumonitor.cpp \
    main.cpp \
    robotview/rrglwidget.cpp \
    robotview/stlfileloader.cpp \
    zmcaux.cpp \
    common/IniHandler.cpp \
    common/Worker.cpp \
    common/WorkerManager.cpp \
    common/helper.cpp \


HEADERS += \
    CoverPlateFSM.h \
    CrashHandler.h \
    CrashHandlerDbg.h \
    DataType.h \
    DatabaseManager.h \
    LRUdatadef.h \
    PMC_lib/PMC006xxLib.h \
    PMC_lib/PUSIControlLib.h \
    SCHeadData.h \
    STLModel/coverplatewidget.h \
    STLModel/newform.h \
    STLModel/rrglwidget.h \
    STLModel/stlfileloader.h \
    SafetyMonitor.h \
    SerialSensor.h \
    SharedMemory/sharedmemorymanager.h \
    StepperMotor.h \
    VehStatemachine.h \
    SCStatusTcp.h \
    TCPSensor.h \
    mainwindow.h \
    motorcontroller.h \
    sriSensorDataDefine.h \
    sriSensorManager.h \
    srisensorworker.h \
    uvc_camera.h \
    vision/code/lenslic/ConfigVal.h \
    vision/code/lenslic/DefVal.h \
    vision/code/lenslic/LensConnect.h \
    vision/code/lenslic/LensCtrl.h \
    vision/code/lenslic/SLABCP2112.h \
    vision/code/lenslic/Types.h \
    vision/code/lenslic/devAddr.h \
    vision_source/CDeviceProcess.h \
    vision_source/CGXBitmap.h \
    vision_source/arucoDetector.h \
    vision_source/dahengTwoCams_qt_vs.h \
    vision_source/dmdetected.h \
    vision_source/guangzilasersensor.h \
    vision_source/imageviewer.h \
    vision_source/lightsourcecontroller.h \
    vision_source/udpcommunication.h \
    zmotioncontrol.h \
    agvmonitor.h \
    levelmonitor.h \
    liftmonitor.h \
    coverplatemonitor.h \
    pinmonitor.h \
    SCVehicle.h \
    LevelingSystem.h \
    MainLift.h \
    CoverPlate.h \
    Lift.h \
    Pin.h \
    TaskMgr.h \
    loghandler.h \
    logmgr.h \
    LRU.h \
    lrumonitor.h \
    robotview/rrglwidget.h \
    robotview/stlfileloader.h \
    zauxdll2.h \
    zmotion.h \
    zmcaux.h \
    common/IniHandler.h \
    common/Worker.h \
    common/WorkerManager.h \
    common/helper.h \


FORMS += lrumonitor.ui \
    STLModel/newform.ui \
    agvmonitor.ui \
    coverplatemonitor.ui \
    levelmonitor.ui \
    liftmonitor.ui \
    mainwindow.ui \
    pinmonitor.ui \
    vision_source/CGXBitmap.ui \
    vision_source/dahengTwoCams_qt_vs.ui
FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target





# win32: LIBS += -L$$PWD/./ -lzmotion\
#             -lOpenGL32\
#             -lGlU32


win32 {
    # 库文件搜索路径
    LIBS += -L$$PWD/ -lzmotion \
            -lOpenGL32 \
            -lGlU32

    LIBS += -L$$PWD/ -liDAS

    LIBS += -L$$PWD/vision/code/lib        # SLABHID 和 OpenCV DLL 目录
    LIBS += -L$$PWD/vision/code/opencv4.10install/x86/vc16/lib  # OpenCV 库文件目录

    # 链接库文件
    LIBS += -lSLABHIDtoSMBus -lGxIAPICPPEx

    #PMC 电机
    LIBS += -L$$PWD/PMC_lib/ -lPUSIControl

    # OpenCV 库 - 根据构建类型链接不同库
    win32:CONFIG(release, debug|release): LIBS += -lopencv_world4100
    win32:CONFIG(debug, debug|release): LIBS += -lopencv_world4100d

    # 构建后复制 DLL 到输出目录（已修正命令分隔方式）
    QMAKE_POST_LINK = $$quote(copy /Y \"$$PWD\\vision\\code\\lib\\SLABHIDDevice.dll\" \"$$OUT_PWD\")
    QMAKE_POST_LINK += $$quote(&& copy /Y \"$$PWD\\vision\\code\\lib\\SLABHIDtoSMBus.dll\" \"$$OUT_PWD\")
    QMAKE_POST_LINK += $$quote(&& copy /Y \"$$PWD\\vision\\code\\lib\\opencv_world4100d.dll\" \"$$OUT_PWD\")
    QMAKE_POST_LINK += $$quote(&& copy /Y \"$$PWD\\vision\\code\\lib\\opencv_world4100.dll\" \"$$OUT_PWD\")
    QMAKE_POST_LINK += $$quote(&& copy /Y \"$$PWD\\vision\\code\\lib\\opencv_videoio_ffmpeg4100.dll\" \"$$OUT_PWD\")

    # 配置安装目标
    deploy.files = \
        $$PWD/vision/code/lib/SLABHIDDevice.dll \
        $$PWD/vision/code/lib/SLABHIDtoSMBus.dll \
        $$PWD/vision/code/lib/opencv_world4100d.dll \
        $$PWD/vision/code/lib/opencv_world4100.dll \
        $$PWD/vision/code/lib/opencv_videoio_ffmpeg4100.dll
    deploy.path = $$OUT_PWD
    INSTALLS += deploy
}

INCLUDEPATH += $$PWD/./bin/eigen-3.4.0
INCLUDEPATH += $$PWD/vision/code/lenslic
INCLUDEPATH += $$PWD/vision/code/opencv4.10install/include
INCLUDEPATH += $$PWD/vision/code/dahenginc
INCLUDEPATH += $$PWD/vision/code/ffmpeg/include
DEPENDPATH += $$PWD/.



LIBS += $$PWD/vision/code/ffmpeg/lib/avcodec.lib \
        $$PWD/vision/code/ffmpeg/lib/avdevice.lib \
        $$PWD/vision/code/ffmpeg/lib/avfilter.lib \
        $$PWD/vision/code/ffmpeg/lib/avformat.lib \
        $$PWD/vision/code/ffmpeg/lib/avutil.lib \
        $$PWD/vision/code/ffmpeg/lib/swresample.lib \
        $$PWD/vision/code/ffmpeg/lib/swscale.lib


# 新增
INCLUDEPATH += $$PWD/ThirdLibs/hp-socket-6.0.3-lib-win/Include \
               $$PWD/ThirdLibs \
               $$PWD/ThirdLibs/boost_1_85_0

# win32:CONFIG(debug, debug|release): {
# #LIBS += -L$$PWD/ThirdLibs/hp-socket-6.0.3-lib-win/Lib/HPSocket/x86/ -lHPSocket_UD
# LIBS += -L$$PWD/ThirdLibs/boost_1_85_0/lib/ -lboost_date_time-vc142-mt-gd-x32-1_85
# LIBS += -L$$PWD/ThirdLibs/boost_1_85_0/lib/ -lboost_thread-vc142-mt-gd-x32-1_85
# } else:win32:CONFIG(release, debug|release): {
# #LIBS += -LC$$PWD/ThirdLibs/hp-socket-6.0.3-lib-win/Lib/HPSocket/x86/ -lHPSocket_U
# LIBS += -L$$PWD/ThirdLibs/boost_1_85_0/lib/ -lboost_date_time-vc142-mt-x32-1_85
# LIBS += -L$$PWD/ThirdLibs/boost_1_85_0/lib/ -lboost_thread-vc142-mt-x32-1_85
# }

win32:LIBS += -lDbgHelp

RESOURCES += \
    QSS.qrc \


