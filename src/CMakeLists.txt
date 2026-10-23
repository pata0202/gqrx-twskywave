###############################################################################
# Add the source subdirectories
add_subdirectory(applications)
add_subdirectory(dsp)
add_subdirectory(receivers)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    if(${OSX_AUDIO_BACKEND} MATCHES "Portaudio")
        add_subdirectory(portaudio)
    else()
        add_subdirectory(osxaudio)
    endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
    if(${LINUX_AUDIO_BACKEND} MATCHES "Pulseaudio")
        add_subdirectory(pulseaudio)
    elseif(${LINUX_AUDIO_BACKEND} MATCHES "Portaudio")
        add_subdirectory(portaudio)
    endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
endif()

###############################################################################
# bring in the global properties
get_property(${PROJECT_NAME}_SOURCE GLOBAL PROPERTY SRCS_LIST)

###############################################################################
# Build the program
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCE})
if(Qt6_FOUND)
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
else()
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
endif()
# The pulse libraries are only needed on Linux. On other platforms they will
# not be found, so having them here is fine.

if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Network
    )
else()
    target_link_libraries(${PROJECT_NAME}
        Qt5::Core
        Qt5::Network
    )
endif()

target_link_libraries(${PROJECT_NAME}
    ${GNURADIO_OSMOSDR_LIBRARIES}
    ${PULSEAUDIO_LIBRARY}
    ${PULSE-SIMPLE}
    ${PORTAUDIO_LIBRARIES}
)

target_link_libraries(${PROJECT_NAME}
    gnuradio::gnuradio-analog
    gnuradio::gnuradio-blocks
    gnuradio::gnuradio-digital
    gnuradio::gnuradio-filter
    gnuradio::gnuradio-audio
    Volk::volk
)

#build a win32 app, not a console app
if (WIN32)
    if (MSVC)
        set(CMAKE_EXE_LINKER_FLAGS "/entry:mainCRTStartup ${CMAKE_EXE_LINKER_FLAGS}")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "-Wl,-e,mainCRTStartup ${CMAKE_EXE_LINKER_FLAGS}")
    endif(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON)
endif (WIN32)

set(INSTALL_DEFAULT_BINDIR "bin" CACHE STRING "Appended to CMAKE_INSTALL_PREFIX")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${INSTALL_DEFAULT_BINDIR})

# Self-check for the noise reduction block: run with ctest
add_executable(rx_nr_ff_test dsp/rx_nr_ff_test.cpp dsp/rx_nr_ff.cpp)
target_link_libraries(rx_nr_ff_test gnuradio::gnuradio-runtime gnuradio::gnuradio-fft)
add_test(NAME rx_nr_ff COMMAND rx_nr_ff_test)
