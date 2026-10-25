cmake_minimum_required(VERSION 3.7.0)

project(Attorney_Online VERSION 2.11.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(AO_ENABLE_DISCORD_RPC "Enable Discord Rich Presence" ON)

find_package(QT NAMES Qt6 Core Gui MultimediaWidgets Network Widgets Concurrent WebSockets UiTools)
find_package(Qt6 REQUIRED COMPONENTS Core Gui MultimediaWidgets Network Widgets Concurrent WebSockets UiTools)

qt_add_executable(Attorney_Online
  src/main.cpp
  src/aoapplication.h
  src/aoblipplayer.h
  src/aobutton.h
  src/aocharbutton.h
  src/aoclocklabel.h
  src/aoemotebutton.h
  src/aoemotepreview.h
  src/aoevidencebutton.h
  src/aoevidencedisplay.h
  src/aographicsview.h
  src/aoimage.h
  src/aolayer.h
  src/aomusicplayer.h
  src/aopacket.h
  src/aosfxplayer.h
  src/aotextarea.h
  src/aoutils.h
  src/chatlogpiece.h
  src/courtroom.h
  src/datatypes.h
  src/debug_functions.h
  src/demoserver.h
  src/eventfilters.h
  src/file_functions.h
  src/hardware_functions.h
  src/lobby.h
  src/mediatester.h
  src/misc_functions.h
  src/moderation_functions.h
  src/networkmanager.h
  src/options.h
  src/scrolltext.h
  src/text_file_functions.h
  src/interfaces/server_dialog.h
  src/video/videoscreen.h
  src/widgets/add_server_dialog.h
  src/widgets/aooptionsdialog.h
  src/widgets/direct_connect_dialog.h
  src/widgets/edit_server_dialog.h
  src/widgets/moderator_dialog.h
  src/widgets/playerlistwidget.h
src/aoapplication.cpp
  src/aoblipplayer.cpp
  src/aobutton.cpp
  src/aocharbutton.cpp
  src/aoclocklabel.cpp
  src/aoemotebutton.cpp
  src/aoemotepreview.cpp
  src/aoevidencebutton.cpp
  src/aoevidencedisplay.cpp
  src/aographicsview.cpp
  src/aoimage.cpp
  src/aolayer.cpp
  src/aomusicplayer.cpp
  src/aopacket.cpp
  src/aosfxplayer.cpp
  src/aotextarea.cpp
  src/aoutils.cpp
  src/charselect.cpp
  src/chatlogpiece.cpp
  src/courtroom.cpp
  src/debug_functions.cpp
  src/demoserver.cpp
  src/discord_rich_presence.cpp
  src/emotes.cpp
  src/evidence.cpp
  src/file_functions.cpp
  src/hardware_functions.cpp
  src/lobby.cpp
  src/mediatester.cpp
  src/misc_functions.cpp
  src/moderation_functions.cpp
  src/networkmanager.cpp
  src/options.cpp
  src/packet_distribution.cpp
  src/path_functions.cpp
  src/scrolltext.cpp
  src/text_file_functions.cpp
  src/video/videoscreen.cpp
  src/widgets/add_server_dialog.cpp
  src/widgets/aooptionsdialog.cpp
  src/widgets/direct_connect_dialog.cpp
  src/widgets/edit_server_dialog.cpp
  src/widgets/moderator_dialog.cpp
  src/widgets/playerlistwidget.cpp
  resources.qrc
)

target_include_directories(Attorney_Online PRIVATE src lib)
target_link_directories(Attorney_Online PRIVATE lib)
target_link_libraries(Attorney_Online PRIVATE
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::MultimediaWidgets
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Concurrent
  Qt${QT_VERSION_MAJOR}::WebSockets
  Qt${QT_VERSION_MAJOR}::UiTools
  bass
  bassopus
  bassmidi
)

if(AO_ENABLE_DISCORD_RPC)
  target_compile_definitions(Attorney_Online PRIVATE AO_ENABLE_DISCORD_RPC)
  target_link_libraries(Attorney_Online PRIVATE discord-rpc)
endif()

set_target_properties(Attorney_Online PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(WIN32)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET Attorney_Online PROPERTY WIN32_EXECUTABLE true)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resource/logo-client.rc")
    target_sources(Attorney_Online PRIVATE ${APP_ICON_RESOURCE_WINDOWS})
  endif()
endif()