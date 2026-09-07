cmake_minimum_required(VERSION 3.16)

project(PlantMap VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 每次编译后的可执行文件统一放到项目根目录 bin/，方便直接打开查看。
set(PLANTMAP_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin" CACHE PATH
    "PlantMap 可执行文件输出目录")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 核心数据层：不依赖 Widgets，方便以后做命令行工具/自检
add_library(plantcore STATIC
    src/core/dbversion.cpp
    src/core/dbversion.h
    src/core/taxonrank.cpp
    src/core/speciesinfo.cpp
    src/core/taxondocument.cpp
)
target_include_directories(plantcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(plantcore PUBLIC Qt6::Core)

# 桌面应用
qt_add_executable(PlantMap
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/speciesview.cpp
    src/ui/speciesview.h
    src/ui/speciesform.cpp
    src/ui/speciesform.h
)
target_include_directories(PlantMap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)
target_link_libraries(PlantMap PRIVATE plantcore Qt6::Widgets)
target_compile_definitions(PlantMap PRIVATE
    PLANTMAP_VERSION="${PROJECT_VERSION}"
)
set_target_properties(PlantMap PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    RUNTIME_OUTPUT_DIRECTORY "${PLANTMAP_BIN_DIR}"
)

# 自动把 Qt 运行库部署到 bin/，保证 exe 可直接双击运行。
if(WIN32)
    get_target_property(_qt_qmake_location Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_location}" DIRECTORY)
    find_program(_qt_windeployqt windeployqt HINTS "${_qt_bin_dir}")
    if(_qt_windeployqt)
        add_custom_command(TARGET PlantMap POST_BUILD
            COMMAND "${_qt_windeployqt}" --no-translations --compiler-runtime
                    "$<TARGET_FILE:PlantMap>"
            COMMENT "正在把 Qt 运行库部署到 bin/ 目录"
        )
    endif()
endif()

# 控制台自检程序，验证分类树 / 拉丁名唯一性 / JSON 往返
qt_add_executable(plantmap_selftest
    tests/selftest.cpp
)
target_link_libraries(plantmap_selftest PRIVATE plantcore)
set_target_properties(plantmap_selftest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PLANTMAP_BIN_DIR}"
)

include(GNUInstallDirs)
install(TARGETS PlantMap
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
