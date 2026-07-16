cmake_minimum_required(VERSION 3.16)
project(FaceRecognition6666 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql)

find_package(OpenCV 4.5 REQUIRED COMPONENTS
    core
    imgproc
    imgcodecs
    videoio
    objdetect
)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
set(SOURCES
    main.cpp
    mainwindow.cpp
    facerecognizer.cpp
    facedatabase.cpp
    personmanagerdialog.cpp
    facesamplestorage.cpp
    recognitionlogstorage.cpp
    appdatapaths.cpp
)

set(HEADERS
    mainwindow.h
    facerecognizer.h
    facedatabase.h
    personmanagerdialog.h
    facesamplestorage.h
    recognitionlogstorage.h
    appdatapaths.h
)

set(UI_FILES
    mainwindow.ui
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    ${OpenCV_LIBS}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})

#CTest自动测试
include(CTest)
option(ENABLE_FACE_IMAGE_TESTS "Enable face detector/feature tests that require user-provided images" OFF)
set(FACE_TEST_IMAGES "" CACHE STRING "Semicolon-separated face image paths used only by optional tests")

if(BUILD_TESTING)
    add_executable(face_database_test
        tests/face_database_test.cpp
        facedatabase.cpp
        facedatabase.h
        facesamplestorage.cpp
        facesamplestorage.h
        recognitionlogstorage.cpp
        recognitionlogstorage.h
        appdatapaths.cpp
        appdatapaths.h
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

    if(ENABLE_FACE_IMAGE_TESTS)
        if(NOT FACE_TEST_IMAGES)
            message(FATAL_ERROR "ENABLE_FACE_IMAGE_TESTS is ON, but FACE_TEST_IMAGES is empty. ""Pass one or more image paths with -DFACE_TEST_IMAGES=...")
        endif()

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

        set(face_test_index 0)
        foreach(source_image IN LISTS FACE_TEST_IMAGES)
            if(NOT EXISTS "${source_image}")
                message(FATAL_ERROR "Face test image does not exist: ${source_image}")
            endif()
            math(EXPR face_test_index "${face_test_index} + 1")
            get_filename_component(image_extension "${source_image}" EXT)
            set(copied_image "${CMAKE_CURRENT_BINARY_DIR}/testdata/face_${face_test_index}${image_extension}")
            configure_file("${source_image}" "${copied_image}" COPYONLY)
            add_test(
                NAME face_detector_${face_test_index}
                COMMAND face_detector_smoke_test
                    "${CMAKE_CURRENT_SOURCE_DIR}/models/face_detection_yunet_2023mar.onnx"
                    "${CMAKE_CURRENT_SOURCE_DIR}/models/face_recognition_sface_2021dec.onnx"
                    "${copied_image}"
            )
            add_test(
                NAME face_feature_${face_test_index}
                COMMAND face_feature_smoke_test
                    "${CMAKE_CURRENT_SOURCE_DIR}/models/face_detection_yunet_2023mar.onnx"
                    "${CMAKE_CURRENT_SOURCE_DIR}/models/face_recognition_sface_2021dec.onnx"
                    "${copied_image}"
            )
        endforeach()
    endif()
endif()


set(FACE_MODEL_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/models/face_detection_yunet_2023mar.onnx"
    "${CMAKE_CURRENT_SOURCE_DIR}/models/face_recognition_sface_2021dec.onnx"
)
foreach(model_file IN LISTS FACE_MODEL_FILES)
    if(NOT EXISTS "${model_file}")
        message(FATAL_ERROR "Required face model is missing: ${model_file}")
    endif()
endforeach()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FACE_MODEL_FILES}
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/models"
    COMMENT "Copying model files to output directory"
)

#寻找opencv路径
set(OpenCV_RUNTIME_DIR "" CACHE PATH "Optional directory containing OpenCV runtime DLLs")
set(_opencv_runtime_dir "${OpenCV_RUNTIME_DIR}")
if(WIN32 AND NOT _opencv_runtime_dir)
    get_filename_component(_opencv_default_runtime_dir "${OpenCV_DIR}/../bin" ABSOLUTE)
    if(EXISTS "${_opencv_default_runtime_dir}")
        set(_opencv_runtime_dir "${_opencv_default_runtime_dir}")
    endif()
endif()

if(WIN32 AND _opencv_runtime_dir)
    file(GLOB _opencv_runtime_dlls
        "${_opencv_runtime_dir}/opencv_world*.dll"
        "${_opencv_runtime_dir}/opencv_videoio_ffmpeg*.dll")
    if(_opencv_runtime_dlls)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${_opencv_runtime_dlls}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copying OpenCV runtime DLLs"
        )
    else()
        message(WARNING
            "No OpenCV runtime DLLs found in ${_opencv_runtime_dir}. "
            "Add the OpenCV bin directory to PATH or set OpenCV_RUNTIME_DIR.")
    endif()
endif()
