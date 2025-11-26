cmake_minimum_required(VERSION 3.16...3.28)

project(keyhunt
    VERSION 0.3.0
    DESCRIPTION "Bitcoin Puzzle Hunter - Optimized for Apple Silicon"
    LANGUAGES C CXX
)

# ============================================================================
# Apple Silicon Focused Build
# ============================================================================
# This project is optimized for macOS Apple Silicon (M1/M2/M3/M4)
# The ARM64 architecture with unified memory makes it ideal for
# secp256k1 elliptic curve computations needed for Bitcoin puzzle hunting.

option(KEYHUNT_BUILD_TESTS "Build test executables" OFF)
option(KEYHUNT_USE_OPENMP "Enable OpenMP for parallel processing" ON)
option(KEYHUNT_ENABLE_LTO "Enable Link Time Optimization" ON)
option(KEYHUNT_BUILD_BSGSD "Build BSGS daemon executable" ON)
option(KEYHUNT_APPLE_SILICON_ONLY "Optimize exclusively for Apple Silicon" ON)
option(KEYHUNT_USE_CUDA "Enable NVIDIA CUDA GPU acceleration" OFF)

# ============================================================================
# CUDA Configuration
# ============================================================================
if(KEYHUNT_USE_CUDA)
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        find_package(CUDAToolkit REQUIRED)
        set(CMAKE_CUDA_STANDARD 17)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)

        # Get CUDA architecture from toolkit or use defaults
        if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
            # Support Turing (75), Ampere (80, 86), Ada Lovelace (89), Hopper (90)
            set(CMAKE_CUDA_ARCHITECTURES 75 80 86 89 90)
        endif()

        message(STATUS "CUDA enabled: ${CMAKE_CUDA_COMPILER_VERSION}")
        message(STATUS "CUDA architectures: ${CMAKE_CUDA_ARCHITECTURES}")
        set(KEYHUNT_CUDA_FOUND TRUE)
    else()
        message(WARNING "CUDA requested but no CUDA compiler found")
        set(KEYHUNT_CUDA_FOUND FALSE)
    endif()
endif()

# ============================================================================
# C++ Standard Configuration
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================================
# Build Type Configuration
# ============================================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

# ============================================================================
# Compiler-Specific Optimizations
# ============================================================================
include(CheckCXXCompilerFlag)

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(KEYHUNT_ARCH_ARM64 TRUE)
    message(STATUS "Architecture: ARM64 (Apple Silicon)")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(KEYHUNT_ARCH_X64 TRUE)
    message(STATUS "Architecture: x86_64")
endif()

# Apple Silicon specific check
if(APPLE AND NOT KEYHUNT_ARCH_ARM64 AND KEYHUNT_APPLE_SILICON_ONLY)
    message(WARNING "This build is optimized for Apple Silicon (M1/M2/M3/M4)")
    message(WARNING "Running on Intel Mac may have reduced performance")
endif()

# Set optimization flags - AGGRESSIVE for Apple Silicon
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        # Aggressive optimizations for puzzle hunting
        add_compile_options(-Ofast -ftree-vectorize -ffast-math -funroll-loops)

        # Apple Silicon specific optimizations
        if(APPLE AND KEYHUNT_ARCH_ARM64)
            message(STATUS "Enabling Apple Silicon optimizations")
            add_compile_options(
                -mcpu=apple-m1        # Optimize for M1+ architecture
                -mtune=native         # Tune for the specific chip
                -fvectorize           # Enable auto-vectorization
                -fslp-vectorize       # Enable SLP vectorization
            )
            # Use Apple's Accelerate framework for math operations
            add_compile_definitions(ACCELERATE_NEW_LAPACK)
        elseif(KEYHUNT_ARCH_ARM64)
            check_cxx_compiler_flag("-mcpu=native" COMPILER_SUPPORTS_MCPU_NATIVE)
            if(COMPILER_SUPPORTS_MCPU_NATIVE)
                add_compile_options(-mcpu=native)
            endif()
        elseif(KEYHUNT_ARCH_X64)
            check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
            if(COMPILER_SUPPORTS_MARCH_NATIVE)
                add_compile_options(-march=native)
            endif()
        endif()
    elseif(MSVC)
        add_compile_options(/O2 /Oi /Ot /GL)
        add_link_options(/LTCG)
    endif()
endif()

# Link Time Optimization
if(KEYHUNT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
    if(LTO_SUPPORTED)
        message(STATUS "LTO enabled")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO not supported: ${LTO_ERROR}")
    endif()
endif()

# ============================================================================
# Find Dependencies
# ============================================================================
find_package(Threads REQUIRED)

# OpenSSL
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
endif()

# GMP (GNU Multiple Precision Arithmetic Library)
find_library(GMP_LIBRARY NAMES gmp libgmp
    HINTS
        /opt/homebrew/lib
        /usr/local/lib
        /usr/lib
        /usr/lib/x86_64-linux-gnu
)
find_path(GMP_INCLUDE_DIR gmp.h
    HINTS
        /opt/homebrew/include
        /usr/local/include
        /usr/include
)

if(NOT GMP_LIBRARY OR NOT GMP_INCLUDE_DIR)
    message(FATAL_ERROR "GMP library not found. Please install libgmp-dev")
endif()
message(STATUS "GMP found: ${GMP_LIBRARY}")

# OpenMP (optional)
if(KEYHUNT_USE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
    else()
        message(WARNING "OpenMP not found, parallel processing disabled")
    endif()
endif()

# ============================================================================
# Source Files
# ============================================================================

# Base58 library
set(BASE58_SOURCES
    base58/base58.c
)

# XXHash library
set(XXHASH_SOURCES
    xxhash/xxhash.c
)

# SHA3/Keccak library
set(SHA3_SOURCES
    sha3/sha3.c
    sha3/keccak.c
)

# RIPEMD160 library
set(RMD160_SOURCES
    rmd160/rmd160.c
)

# Bloom filter libraries
set(BLOOM_SOURCES
    bloom/bloom.cpp
    oldbloom/bloom.cpp
)

# Hash functions
set(HASH_SOURCES
    hash/sha256.cpp
    hash/sha512.cpp
    hash/ripemd160.cpp
)

# SSE optimized hash functions (x86_64 only)
if(KEYHUNT_ARCH_X64)
    list(APPEND HASH_SOURCES
        hash/sha256_sse.cpp
        hash/ripemd160_sse.cpp
    )
    message(STATUS "Including SSE-optimized hash functions")
endif()

# GMP256K1 (GMP-based secp256k1)
set(GMP256K1_SOURCES
    gmp256k1/Int.cpp
    gmp256k1/IntMod.cpp
    gmp256k1/IntGroup.cpp
    gmp256k1/Point.cpp
    gmp256k1/GMP256K1.cpp
    gmp256k1/Random.cpp
)

# SECP256K1 library
set(SECP256K1_SOURCES
    secp256k1/Int.cpp
    secp256k1/IntMod.cpp
    secp256k1/IntGroup.cpp
    secp256k1/Point.cpp
    secp256k1/SECP256K1.cpp
    secp256k1/Random.cpp
)

# Utility sources
set(UTIL_SOURCES
    util.c
    hashing.c
)

# ============================================================================
# Static Libraries
# ============================================================================

# Crypto utilities library
add_library(keyhunt_crypto STATIC
    ${BASE58_SOURCES}
    ${XXHASH_SOURCES}
    ${SHA3_SOURCES}
    ${RMD160_SOURCES}
    ${HASH_SOURCES}
    ${BLOOM_SOURCES}
    ${UTIL_SOURCES}
)

target_include_directories(keyhunt_crypto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GMP_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(keyhunt_crypto PUBLIC
    ${GMP_LIBRARY}
    OpenSSL::Crypto
    Threads::Threads
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(keyhunt_crypto PUBLIC OpenMP::OpenMP_CXX)
endif()

# GMP256K1 library (for legacy keyhunt)
add_library(gmp256k1 STATIC ${GMP256K1_SOURCES})

target_include_directories(gmp256k1 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gmp256k1
    ${GMP_INCLUDE_DIR}
)

target_link_libraries(gmp256k1 PUBLIC
    ${GMP_LIBRARY}
    keyhunt_crypto
)

# SECP256K1 library (for bsgsd)
add_library(secp256k1_lib STATIC ${SECP256K1_SOURCES})

target_include_directories(secp256k1_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1
    ${GMP_INCLUDE_DIR}
)

target_link_libraries(secp256k1_lib PUBLIC
    ${GMP_LIBRARY}
    keyhunt_crypto
)

# ============================================================================
# CUDA Library (32-bit limb secp256k1 for GPU)
# ============================================================================
if(KEYHUNT_CUDA_FOUND)
    set(CUDA_SOURCES
        cuda/bsgs_kernel.cu
    )

    add_library(keyhunt_cuda STATIC ${CUDA_SOURCES})

    target_include_directories(keyhunt_cuda PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/cuda
    )

    # CUDA-specific compile options for maximum performance
    set_target_properties(keyhunt_cuda PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(keyhunt_cuda PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--use_fast_math>
        $<$<COMPILE_LANGUAGE:CUDA>:-O3>
        $<$<COMPILE_LANGUAGE:CUDA>:--ptxas-options=-v>
    )

    target_link_libraries(keyhunt_cuda PUBLIC
        CUDA::cudart
    )

    # Define CUDA_ENABLED for conditional compilation
    target_compile_definitions(keyhunt_cuda PUBLIC CUDA_ENABLED)

    message(STATUS "CUDA library: keyhunt_cuda")
endif()

# ============================================================================
# Executables
# ============================================================================

# Main keyhunt executable (legacy mode)
add_executable(keyhunt keyhunt_legacy.cpp)

target_include_directories(keyhunt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GMP_INCLUDE_DIR}
)

target_link_libraries(keyhunt PRIVATE
    gmp256k1
    keyhunt_crypto
    ${GMP_LIBRARY}
    OpenSSL::Crypto
    Threads::Threads
    m
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(keyhunt PRIVATE OpenMP::OpenMP_CXX)
endif()

if(KEYHUNT_CUDA_FOUND)
    target_link_libraries(keyhunt PRIVATE keyhunt_cuda)
    target_compile_definitions(keyhunt PRIVATE CUDA_ENABLED)
endif()

# BSGS Daemon executable
if(KEYHUNT_BUILD_BSGSD)
    add_executable(bsgsd bsgsd.cpp)

    target_include_directories(bsgsd PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GMP_INCLUDE_DIR}
    )

    target_link_libraries(bsgsd PRIVATE
        secp256k1_lib
        keyhunt_crypto
        ${GMP_LIBRARY}
        OpenSSL::Crypto
        Threads::Threads
        m
    )

    if(OpenMP_CXX_FOUND)
        target_link_libraries(bsgsd PRIVATE OpenMP::OpenMP_CXX)
    endif()
endif()

# ============================================================================
# Platform-Specific Settings
# ============================================================================
if(WIN32)
    target_compile_definitions(keyhunt PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(KEYHUNT_BUILD_BSGSD)
        target_compile_definitions(bsgsd PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

if(APPLE)
    # macOS specific settings
    target_compile_definitions(keyhunt PRIVATE __APPLE__)
    if(KEYHUNT_BUILD_BSGSD)
        target_compile_definitions(bsgsd PRIVATE __APPLE__)
    endif()
endif()

if(UNIX AND NOT APPLE)
    # Linux specific settings
    target_link_libraries(keyhunt PRIVATE ${CMAKE_DL_LIBS})
    if(KEYHUNT_BUILD_BSGSD)
        target_link_libraries(bsgsd PRIVATE ${CMAKE_DL_LIBS})
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================
include(GNUInstallDirs)

install(TARGETS keyhunt
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(KEYHUNT_BUILD_BSGSD)
    install(TARGETS bsgsd
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# Install test files
install(DIRECTORY tests/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/keyhunt/tests
    FILES_MATCHING PATTERN "*.txt" PATTERN "*.rmd" PATTERN "*.pub"
)

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Keyhunt Configuration Summary")
message(STATUS "========================================")
message(STATUS "Version:        ${PROJECT_VERSION}")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "LTO:            ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
message(STATUS "OpenMP:         ${OpenMP_CXX_FOUND}")
message(STATUS "OpenSSL:        ${OPENSSL_VERSION}")
message(STATUS "GMP:            ${GMP_LIBRARY}")
message(STATUS "Build BSGSD:    ${KEYHUNT_BUILD_BSGSD}")
if(KEYHUNT_USE_CUDA)
    message(STATUS "CUDA:           ${KEYHUNT_CUDA_FOUND} (${CMAKE_CUDA_COMPILER_VERSION})")
    message(STATUS "CUDA Archs:     ${CMAKE_CUDA_ARCHITECTURES}")
else()
    message(STATUS "CUDA:           OFF")
endif()
message(STATUS "========================================")
message(STATUS "")
