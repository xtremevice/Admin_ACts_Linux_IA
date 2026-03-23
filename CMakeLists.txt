cmake_minimum_required(VERSION 3.10)
project(AdminActsLinuxIA VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/UpdatesTab.cpp
    src/InstallTab.cpp
    src/SettingsTab.cpp
    src/PackageManager.cpp
    src/AuthDialog.cpp
)

add_executable(admin-acts-linux ${SOURCES})

target_include_directories(admin-acts-linux PRIVATE
    ${GTKMM_INCLUDE_DIRS}
    src/
)

target_link_directories(admin-acts-linux PRIVATE
    ${GTKMM_LIBRARY_DIRS}
)

target_compile_options(admin-acts-linux PRIVATE
    ${GTKMM_CFLAGS_OTHER}
)

target_link_libraries(admin-acts-linux
    ${GTKMM_LIBRARIES}
    stdc++fs
)

install(TARGETS admin-acts-linux DESTINATION bin)
install(FILES data/admin-acts-linux.desktop DESTINATION share/applications OPTIONAL)
