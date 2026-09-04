cmake_minimum_required(VERSION 3.16)

project(PlantMap VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 核心数据层：不依赖 Widgets，方便以后做命令行工具/自检
add_library(plantcore STATIC
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
    src/ui/speciesform.cpp
    src/ui/speciesform.h
)
target_include_directories(PlantMap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)
target_link_libraries(PlantMap PRIVATE plantcore Qt6::Widgets)
target_compile_definitions(PlantMap PRIVATE
    PLANTMAP_SOURCE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)
set_target_properties(PlantMap PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

# 控制台自检程序，验证分类树 / 拉丁名唯一性 / JSON 往返
qt_add_executable(plantmap_selftest
    tests/selftest.cpp
)
target_link_libraries(plantmap_selftest PRIVATE plantcore)

include(GNUInstallDirs)
install(TARGETS PlantMap
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
