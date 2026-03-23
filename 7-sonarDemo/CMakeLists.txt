cmake_minimum_required(VERSION 3.5)

project(sonarDemo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets SerialPort Network  REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets SerialPort Network  REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

# Specify MSVC UTF-8 encoding   
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# =========================
# SDK 路径
# =========================
set(SONAR_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
set(SONAR_SDK_INCLUDE_DIR ${SONAR_SDK_DIR}/include)
set(SONAR_SDK_LIB_DIR ${SONAR_SDK_DIR}/lib)

# Windows 下厂家提供的库
set(SONAR_SDK_LIB ${SONAR_SDK_LIB_DIR}/PipesonarSDK.lib)
set(SONAR_SDK_DLL ${SONAR_SDK_LIB_DIR}/PipesonarSDK.dll)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(sonarDemo
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET sonarDemo APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(sonarDemo SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(sonarDemo
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(sonarDemo PRIVATE
    ${SONAR_SDK_INCLUDE_DIR}
)

target_link_libraries(sonarDemo PRIVATE 
                                Qt${QT_VERSION_MAJOR}::Widgets 
                                Qt${QT_VERSION_MAJOR}::SerialPort
                                Qt${QT_VERSION_MAJOR}::Network
                                ${SONAR_SDK_LIB}
                                )

set_target_properties(sonarDemo PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(WIN32)
    add_custom_command(TARGET sonarDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SONAR_SDK_DLL}
            $<TARGET_FILE_DIR:sonarDemo>
    )
endif()



if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(sonarDemo)
endif()
