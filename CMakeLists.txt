cmake_minimum_required(VERSION 3.12)

project(tt-llk VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include GNUInstallDirs for standard directory variables
include(GNUInstallDirs)

# Define the available architectures
set(TT_LLK_ARCHITECTURES "wormhole_b0" "blackhole")

# Function to create interface library for an architecture
function(create_architecture_target arch_name arch_dir)
    # Create interface library target
    add_library(tt-llk-${arch_name} INTERFACE)
    add_library(tt-llk::${arch_name} ALIAS tt-llk-${arch_name})

    # These headers are for the device, not host; will require cross compiling to verify.
    set_target_properties(
        tt-llk-${arch_name}
        PROPERTIES
            VERIFY_INTERFACE_HEADER_SETS FALSE
            EXPORT_NAME ${arch_name}
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    # Glob all header files for this architecture
    file(GLOB_RECURSE ARCH_HEADERS "${arch_dir}/**/*.h")
    list(LENGTH ARCH_HEADERS header_count)

    # Add headers as FILE_SET for proper installation
    target_sources(tt-llk-${arch_name}
        INTERFACE
            FILE_SET jit_api
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}
            FILES ${ARCH_HEADERS}
    )

    # Add include directories
    target_include_directories(tt-llk-${arch_name} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/llk_lib>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}/common/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${arch_dir}>
    )

    message(STATUS "Created target tt-llk::${arch_name} with ${header_count} headers from ${arch_dir}")
endfunction()

# Create targets for each architecture
create_architecture_target("wormhole_b0" "tt_llk_wormhole_b0")
create_architecture_target("blackhole" "tt_llk_blackhole")

# Installation - Install targets with FILE_SET for JIT API
install(TARGETS tt-llk-wormhole_b0
    EXPORT tt-llk-targets
    FILE_SET jit_api
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/tt-metalium/tt_metal/third_party/tt_llk/tt_llk_wormhole_b0
        COMPONENT metalium-runtime
)

install(TARGETS tt-llk-blackhole
    EXPORT tt-llk-targets
    FILE_SET jit_api
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/tt-metalium/tt_metal/third_party/tt_llk/tt_llk_blackhole
        COMPONENT metalium-runtime
)

# Generate and install package config files
include(CMakePackageConfigHelpers)

# Create package config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tt-llk-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/tt-llk-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tt-llk
)

# Create package version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tt-llk-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install package config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tt-llk-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tt-llk-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tt-llk
)

# Install targets
install(EXPORT tt-llk-targets
    FILE tt-llk-targets.cmake
    NAMESPACE tt-llk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tt-llk
)

# Print summary
message(STATUS "")
message(STATUS "tt-llk configuration summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Architectures: ${TT_LLK_ARCHITECTURES}")
message(STATUS "  Install headers:")
message(STATUS "    wormhole_b0: ${CMAKE_INSTALL_LIBEXECDIR}/tt-metalium/tt_metal/third_party/tt_llk/tt_llk_wormhole_b0/")
message(STATUS "    blackhole:   ${CMAKE_INSTALL_LIBEXECDIR}/tt-metalium/tt_metal/third_party/tt_llk/tt_llk_blackhole/")
message(STATUS "")
