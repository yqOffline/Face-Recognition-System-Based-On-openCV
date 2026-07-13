cmake_minimum_required(VERSION 3.16)
project(FaceRecognition6666 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件统一使用 UTF-8。Windows 下 MSVC 默认使用系统代码页，
# 如果不明确指定，中文注释和界面文字可能被错误解析。
if(MSVC)
    add_compile_options(/utf-8)
endif()

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
    personmanagerdialog.cpp
)

set(HEADERS
    mainwindow.h
    facerecognizer.h
    facedatabase.h
    personmanagerdialog.h
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

# ---------- 自动化检测回归测试 ----------
# 将中文文件名的测试图片复制为 ASCII 文件名，避免 Windows 路径编码干扰测试结果。
include(CTest)
if(BUILD_TESTING)
    set(FACE_TEST_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/testdata/single_face.png")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/images/单人.png"
        "${FACE_TEST_IMAGE}"
        COPYONLY
    )

    add_executable(face_detector_smoke_test
        tests/face_detector_smoke_test.cpp
        facerecognizer.cpp
        facerecognizer.h
    )
    target_link_libraries(face_detector_smoke_test PRIVATE ${OpenCV_LIBS})
    target_include_directories(face_detector_smoke_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
    )

    add_test(
        NAME face_detector_single_face
        COMMAND face_detector_smoke_test
            "${CMAKE_CURRENT_SOURCE_DIR}/models/face_detection_yunet_2023mar.onnx"
            "${CMAKE_CURRENT_SOURCE_DIR}/models/face_recognition_sface_2021dec.onnx"
            "${FACE_TEST_IMAGE}"
            "${CMAKE_CURRENT_BINARY_DIR}/face_detector_result.png"
    )

    add_executable(face_feature_smoke_test
        tests/face_feature_smoke_test.cpp
        facerecognizer.cpp
        facerecognizer.h
    )
    target_link_libraries(face_feature_smoke_test PRIVATE ${OpenCV_LIBS})
    target_include_directories(face_feature_smoke_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
    )

    add_test(
        NAME face_feature_single_face
        COMMAND face_feature_smoke_test
            "${CMAKE_CURRENT_SOURCE_DIR}/models/face_detection_yunet_2023mar.onnx"
            "${CMAKE_CURRENT_SOURCE_DIR}/models/face_recognition_sface_2021dec.onnx"
            "${FACE_TEST_IMAGE}"
    )

    add_executable(face_database_test
        tests/face_database_test.cpp
        facedatabase.cpp
        facedatabase.h
    )
    target_link_libraries(face_database_test PRIVATE
        Qt6::Core
        Qt6::Sql
        ${OpenCV_LIBS}
    )
    target_include_directories(face_database_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
    )
    add_test(NAME face_database_crud COMMAND face_database_test)

    # 4500x4500 的证件照用于防止超大图片再次出现漏检。
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/images/yq.jpg")
        set(LARGE_FACE_TEST_IMAGE
            "${CMAKE_CURRENT_BINARY_DIR}/testdata/large_face.jpg")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/images/yq.jpg"
            "${LARGE_FACE_TEST_IMAGE}"
            COPYONLY
        )
        add_test(
            NAME face_detector_large_face
            COMMAND face_detector_smoke_test
                "${CMAKE_CURRENT_SOURCE_DIR}/models/face_detection_yunet_2023mar.onnx"
                "${CMAKE_CURRENT_SOURCE_DIR}/models/face_recognition_sface_2021dec.onnx"
                "${LARGE_FACE_TEST_IMAGE}"
        )
        add_test(
            NAME face_feature_large_face
            COMMAND face_feature_smoke_test
                "${CMAKE_CURRENT_SOURCE_DIR}/models/face_detection_yunet_2023mar.onnx"
                "${CMAKE_CURRENT_SOURCE_DIR}/models/face_recognition_sface_2021dec.onnx"
                "${LARGE_FACE_TEST_IMAGE}"
        )
    endif()
endif()

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
