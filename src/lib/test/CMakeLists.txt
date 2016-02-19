#
# pelican-lofar/lib/test/CMakeLists.txt
#

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ==== Create test utility library.
set(lofarTestLib_src
    src/LofarUdpEmulator.cpp
    src/LofarEmulatorDataSim.cpp
    src/GPU_TestCard.cpp
    src/LofarPipelineTester.cpp
    src/LofarDataBlobGenerator.cpp
    src/PipelineWrapper.cpp
    src/DedispersionDataGenerator.cpp
    src/TestDir.cpp
    src/K7Emulator.cpp
)

# CUDA routines
set(cuda_src
   CudaVectorAdd.cu
   )
if( CUDA_FOUND )
    CUDA_COMPILE( cuda_obj ${cuda_src} )
    list(APPEND lofarTestLib_src 
         src/TestCudaVectorAdd.cpp
        )
endif( CUDA_FOUND )

add_library(lofarTestLib STATIC ${lofarTestLib_src} ${cuda_obj} )
target_link_libraries(lofarTestLib
    pelican-lofar
    ${QT_QTNETWORK_LIBRARY}
    ${QT_QTCORE_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
)

# ==== Create test binary and add it to the cmake test framework.
set(lofarTest_src
    src/CppUnitMain.cpp
    src/GPU_ManagerTest.cpp
    src/GPU_MemoryMapTest.cpp
    src/AdapterTimeSeriesDataSetTest.cpp
    src/BandPassTest.cpp
    src/BinMapTest.cpp
    src/DataStreamingTest.cpp
    src/DedispersionDataAnalysisOutputTest.cpp
    src/DedispersionSpectraTest.cpp
    #src/LockingContainerTest.cpp
    #src/PPF_ChanneliserTest.cpp
    #src/RFI_ClipperTest.cpp
    #src/SpectrumDataSetTest.cpp
)
if(CUDA_FOUND)
    list(APPEND lofarTest_src
            src/GPU_NVidiaTest.cpp
            src/GPU_ParamTest.cpp
            src/DedispersionModuleTest.cpp
        #    src/DedispersionAnalyserTest.cpp
        )
endif(CUDA_FOUND)
#if(HDF5_FOUND)
#if(LOFAR_DAL_FOUND)
#    list(APPEND lofarTest_src
#         src/H5_LofarBFDataWriterTest.cpp
#         src/H5_LofarBFStokesWriterTest.cpp
#         src/H5_LofarBFVoltageWriterTest.cpp
#        )
#endif(LOFAR_DAL_FOUND)
#endif(HDF5_FOUND)
#add_executable(lofarTest ${lofarTest_src})
#set_target_properties(lofarTest PROPERTIES 
#    COMPILE_FLAGS "${OpenMP_CXX_FLAGS}"
#    LINK_FLAGS "${OpenMP_CXX_FLAGS}")
#target_link_libraries(lofarTest
#    pelican-lofar_static
#    lofarTestLib
#    ${PELICAN_TESTUTILS_LIBRARY}
#    ${PELICAN_LIBRARY}
#    ${FFTW3_FFTW_LIBRARY}
#    ${FFTW3_FFTWF_LIBRARY}
#    ${CPPUNIT_LIBRARIES}
#    ${QT_QTCORE_LIBRARY}
#    ${QT_QTNETWORK_LIBRARY}
#    ${QT_QTXML_LIBRARY})
#add_test(lofarTest lofarTest)
#

# ==== Create the KATBURST binary which sends simulated data.
add_executable(K7Emulator src/K7EmulatorMain.cpp)
set_source_files_properties(src/K7Emulator.cpp PROPERTIES COMPILE_FLAGS -std=c++11)
target_link_libraries(K7Emulator
    ${PELICAN_LIBRARY}
    pelican-lofar_static
    ${QT_QTCORE_LIBRARY}
    ${QT_QTNETWORK_LIBRARY}
    ${QT_QTXML_LIBRARY}
    lofarTestLib
)
install(TARGETS "K7Emulator" DESTINATION ${BINARY_INSTALL_DIR})

# ==== Copy files required for testing to the build directory.
include(${CMAKE_SOURCE_DIR}/cmake/CopyFiles.cmake)
copy_files(${CMAKE_CURRENT_SOURCE_DIR}/data/*.* . testLibFiles)
