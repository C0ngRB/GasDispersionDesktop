cmake_minimum_required(VERSION 3.20)
project(GasDispersionDesktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

qt_standard_project_setup()

add_executable(GasDispersionDesktop
  src/main.cpp
  src/MainWindow.h
  src/MainWindow.cpp
  src/Simulator.h
  src/Simulator.cpp
  src/ColorMap.h
  src/ExternalCfdBackend.h
  src/ExternalCfdBackend.cpp
  src/CsvFrameReader.h
  src/CsvFrameReader.cpp
  src/Grid3D.h
  src/ITerrain.h
  src/TerrainFlat.h
  src/TerrainProcedural.h
  src/TerrainDem.h
  src/TerrainDem.cpp
  src/Simulator3D.h
  src/Simulator3D.cpp
  src/ExporterCsv.h
  src/ExporterCsv.cpp
)

target_link_libraries(GasDispersionDesktop PRIVATE Qt6::Widgets Qt6::Core)

include(InstallRequiredSystemLibraries)
install(TARGETS GasDispersionDesktop RUNTIME DESTINATION .)

set(CPACK_PACKAGE_NAME "GasDispersionDesktop")
set(CPACK_PACKAGE_VENDOR "Local")
set(CPACK_PACKAGE_VERSION "0.3.0")
set(CPACK_GENERATOR "NSIS")
set(CPACK_NSIS_DISPLAY_NAME "GasDispersionDesktop")
set(CPACK_NSIS_PACKAGE_NAME "GasDispersionDesktop")
set(CPACK_NSIS_MODIFY_PATH OFF)
include(CPack)
