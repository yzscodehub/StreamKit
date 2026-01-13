# Phoenix Editor - Qt/QML Video Editor
cmake_minimum_required(VERSION 3.21)

project(PhoenixEditor LANGUAGES CXX)

# ============================================================================
# Qt Configuration
# ============================================================================
set(Qt6_DIR "C:/Tools/Qt/6.5.3/msvc2019_64/lib/cmake/Qt6")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Qml 
    Quick 
    QuickControls2
    Multimedia
)

# Qt 6 policy for QML modules
qt_policy(SET QTP0001 NEW)

# ============================================================================
# Source Files
# ============================================================================
set(EDITOR_SOURCES
    src/main.cpp
    src/controllers/project_controller.cpp
    src/controllers/timeline_controller.cpp
    src/controllers/preview_controller.cpp
)

set(EDITOR_HEADERS
    src/controllers/project_controller.hpp
    src/controllers/timeline_controller.hpp
    src/controllers/preview_controller.hpp
    src/theme.hpp
)

# ============================================================================
# Executable
# ============================================================================
qt_add_executable(PhoenixEditor
    ${EDITOR_SOURCES}
    ${EDITOR_HEADERS}
)

# ============================================================================
# QML Module Configuration
# ============================================================================

# Define the QML module
# Note: Theme is now provided as C++ context property, not QML singleton
qt_add_qml_module(PhoenixEditor
    URI Phoenix
    VERSION 1.0
    RESOURCE_PREFIX /
    QML_FILES
        qml/Main.qml
        qml/components/MediaBrowser.qml
        qml/components/VideoPreview.qml
        qml/components/Timeline.qml
        qml/components/TimelineTrack.qml
        qml/components/TimelineClip.qml
        qml/components/TransportControls.qml
        qml/components/Toolbar.qml
        qml/components/PropertyPanel.qml
    # Static resources
    RESOURCES
        resources/icons/play.svg
        resources/icons/pause.svg
        resources/icons/stop.svg
        resources/icons/skip-back.svg
        resources/icons/skip-forward.svg
        resources/icons/cut.svg
        resources/icons/import.svg
        resources/icons/export.svg
        resources/icons/undo.svg
        resources/icons/redo.svg
        resources/icons/zoom-in.svg
        resources/icons/zoom-out.svg
)

# ============================================================================
# Include Directories
# ============================================================================
target_include_directories(PhoenixEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# Link Libraries
# ============================================================================
target_link_libraries(PhoenixEditor PRIVATE
    # Qt
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    
    # Phoenix libraries
    phoenix_core
    phoenix_model
    phoenix_media
    phoenix_engine
)

# ============================================================================
# Compiler Options
# ============================================================================
target_compile_features(PhoenixEditor PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(PhoenixEditor PRIVATE /W4 /utf-8)
endif()

# ============================================================================
# Windows Deployment
# ============================================================================
if(WIN32)
    # Copy Qt DLLs on build
    add_custom_command(TARGET PhoenixEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt6::Core>
            $<TARGET_FILE:Qt6::Gui>
            $<TARGET_FILE:Qt6::Qml>
            $<TARGET_FILE:Qt6::Quick>
            $<TARGET_FILE:Qt6::QuickControls2>
            $<TARGET_FILE:Qt6::Multimedia>
            $<TARGET_FILE_DIR:PhoenixEditor>
    )
endif()

# ============================================================================
# IDE Organization
# ============================================================================
source_group("Source Files" FILES ${EDITOR_SOURCES})
source_group("Header Files" FILES ${EDITOR_HEADERS})
