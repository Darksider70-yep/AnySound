cmake_minimum_required(VERSION 3.25)

find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 QUIET)

if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    add_executable(chorus_desktop
        src/main.cpp
        src/ui_controller.hpp
        src/ui_controller.cpp
    )

    target_link_libraries(chorus_desktop PRIVATE
        chorus_core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
    )

    qt_add_qml_module(chorus_desktop
        URI chorus
        VERSION 1.0
        QML_FILES
            qml/Main.qml
            qml/Theme.qml
            qml/components/SyncBadge.qml
            qml/components/RoomView.qml
            qml/components/VolumeSlider.qml
            qml/components/DelayStepper.qml
            qml/components/DeviceRow.qml
            qml/components/PrimaryButton.qml
            qml/components/SecondaryButton.qml
            qml/views/HomeView.qml
            qml/views/HostView.qml
            qml/views/ClientFindView.qml
            qml/views/ClientListeningView.qml
            qml/views/CalibrateView.qml
            qml/views/SettingsView.qml
    )
else()
    message(STATUS "Qt6 not found on system. chorus_desktop (Qt6 GUI) will be skipped.")
    # Build stub or headless target if needed
    add_executable(chorus_desktop
        src/main.cpp
    )
    target_link_libraries(chorus_desktop PRIVATE
        chorus_core
    )
endif()
