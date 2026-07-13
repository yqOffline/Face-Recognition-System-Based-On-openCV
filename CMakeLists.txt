cmake_minimum_required(VERSION 3.16)
project(FaceRecognition6666 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自动处理 Qt 的 MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---------- 查找 Qt6 ----------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql)

# ---------- 查找 OpenCV ----------
set(OpenCV_DIR "D:/openCV/opencv_4.10.0/opencv/build/x64/vc16/lib")
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")

# ---------- 源文件 ----------
set(SOURCES
    main.cpp
    mainwindow.cpp
    facerecognizer.cpp
    facedatabase.cpp
)

set(HEADERS
    mainwindow.h
    facerecognizer.h
    facedatabase.h
)

# ---------- UI 文件 ----------
set(UI_FILES
    mainwindow.ui
)

# ---------- 可执行文件 ----------
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

# ---------- 链接库 ----------
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    ${OpenCV_LIBS}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})

# ---------- 复制模型文件到输出目录 ----------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/models"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models"
    COMMENT "Copying model files to output directory"
)

# ---------- 复制 OpenCV DLL 到输出目录 ----------
set(OpenCV_DLL_DIR "D:/openCV/opencv_4.10.0/opencv/build/x64/vc16/bin")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${OpenCV_DLL_DIR}/opencv_world4100.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying OpenCV DLL"
)
