# === Set the project name and specify the required CMAKE version.
project(pelican-katburst CXX C Fortran)
cmake_minimum_required(VERSION 2.4)

# === Project version
set(${PROJECT_NAME}_MAJOR_VERSION 0)
set(${PROJECT_NAME}_MINOR_VERSION 0)
set(${PROJECT_NAME}_PATCH_LEVEL 0)

# === Append the src/cmake directory to the module path.
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

# === Enable the cmake testing framework.
enable_testing()

# === Options and defaults.
if(NOT PELICAN_LOFAR_INSTALL_DIR)
    set(PELICAN_LOFAR_INSTALL_DIR "pelican-katburst")
endif(NOT PELICAN_LOFAR_INSTALL_DIR)

if(NOT LIBRARY_INSTALL_DIR)
    set(LIBRARY_INSTALL_DIR "${PELICAN_LOFAR_INSTALL_DIR}/lib")
endif(NOT LIBRARY_INSTALL_DIR)

if(NOT INCLUDE_INSTALL_DIR)
    set(INCLUDE_INSTALL_DIR "${PELICAN_LOFAR_INSTALL_DIR}/include/pelican-katburst")
endif(NOT INCLUDE_INSTALL_DIR)

if(NOT BINARY_INSTALL_DIR)
    set(BINARY_INSTALL_DIR "${PELICAN_LOFAR_INSTALL_DIR}/bin")
endif(NOT BINARY_INSTALL_DIR)

include(compilerDefaults)
include(cmake/tags.cmake)

set(CUDA_PROPAGATE_HOST_FLAGS OFF)
set(CUDA_NVCC_FLAGS --ptxas-options=-v --compiler-options -fPIC)

# === Find Dependencies.
find_package(Pelican REQUIRED)
find_package(CppUnit REQUIRED)
find_package(OpenMP REQUIRED)
find_package(FFTW3 REQUIRED)
find_package(Boost REQUIRED)
find_package(LAPACK REQUIRED)
find_package(LofarDAL REQUIRED)
find_package(HDF5)
if(HDF5_FOUND)
    add_definitions(-DHDF5_FOUND) 
endif(HDF5_FOUND)
find_package(CUDA)
if(CUDA_FOUND)
    add_definitions(-DCUDA_FOUND)
endif(CUDA_FOUND)
if(LOFAR_DAL_FOUND)
    add_definitions(-DLOFAR_DAL_FOUND)
endif(LOFAR_DAL_FOUND)

# === Add directories to the include path (i.e. -I<directory> ...)
include_directories(${PELICAN_INCLUDES})
message(STATUS "AAAAAA ${PELICAN_INCLUDES}")
# == Recurse into sub-directories defined in modules.cmake.
include(modules.cmake)

# === Create the KATBURST server binary.
include_directories(${CMAKE_SOURCE_DIR}/lib)
add_executable(K7Server K7ServerMain.cpp)
target_link_libraries(K7Server
    pelican-lofar_static
    ${PELICAN_LIBRARY}
    ${QT_QTCORE_LIBRARY}
    ${QT_QTXML_LIBRARY}
    ${QT_QTNETWORK_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
)
install(TARGETS K7Server DESTINATION ${BINARY_INSTALL_DIR})

# === Doxygen documentation targets.
include(UseDoxygen)
set(PDFLATEX_COMPILER TRUE)
add_doxygen_target(api DoxyfileAPI)
add_doxygen_target(developer DoxyfileDeveloper)
add_doxygen_target(user DoxyfileUser)
