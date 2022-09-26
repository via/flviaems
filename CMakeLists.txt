cmake_minimum_required(VERSION 3.8)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(flviaems VERSION 0.1
  DESCRIPTION "FLTK ViaEMS Editor"
  LANGUAGES CXX C)

add_executable(flviaems src/TableEditor.cxx src/flviaems.cxx src/StatusTable.cxx
src/MainWindow.cxx src/MainWindowUI.cxx src/viaems.cxx src/Log.cxx
src/LogView.cxx src/OutputEditor.cxx src/LogViewEditor.cxx extern/sqlite/sqlite3.c)

add_definitions(-DSQLITE_THREADSAFE=2)
add_definitions(-DSQLITE_OMIT_LOAD_EXTENSION)
target_compile_features(flviaems PUBLIC cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(flviaems Threads::Threads)

find_library(FLTK fltk)
target_link_libraries(flviaems ${FLTK})

add_subdirectory(extern/json)
target_link_libraries(flviaems nlohmann_json::nlohmann_json)

target_include_directories(flviaems PRIVATE extern/pstreams extern/sqlite)
