cmake_minimum_required(VERSION 3.20)

project(lovepopi_patch)

if (MSVC)
    add_compile_options(/utf-8)
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

set(DETOURS_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/detours.lib")
option(STATIC_CRT "Use static CRT" OFF)

if (STATIC_CRT)
    #add_link_options(/NODEFAULTLIB:libucrt)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(ENABLE_ICONV OFF CACHE BOOL "Libiconv is not needed.")
add_subdirectory(utils)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/utils")

set(ENABLE_COMMONCRYPTO OFF CACHE BOOL "CommonCrypto is not needed.")
set(ENABLE_GNUTLS OFF CACHE BOOL "GnuTLS is not needed.")
set(ENABLE_MBEDTLS OFF CACHE BOOL "MbedTLS is not needed.")
set(ENABLE_OPENSSL OFF CACHE BOOL "OpenSSL is not needed.")
set(ENABLE_WINDOWS_CRYPTO OFF CACHE BOOL "Schannel is not needed.")

set(ENABLE_BZIP2 OFF CACHE BOOL "Bzip2 is not needed.")
set(ENABLE_LZMA OFF CACHE BOOL "Lzma is not needed.")
set(ENABLE_ZSTD OFF CACHE BOOL "Zstd is not needed.")

set(BUILD_TOOLS OFF CACHE BOOL "Tools are not needed.")
set(BUILD_REGRESS OFF CACHE BOOL "Tests are not needed.")
set(BUILD_OSSFUZZ OFF CACHE BOOL "Fuzz tests are not needed.")
set(BUILD_EXAMPLES OFF CACHE BOOL "Examples are not needed.")
set(BUILD_DOC OFF CACHE BOOL "Documentation is not needed.")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static library is needed.")
set(LIBZIP_DO_INSTALL OFF CACHE BOOL "Installation is not needed.")
set(ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Zlib is needed.")

find_package(ZLIB REQUIRED)
add_subdirectory("libzip")

add_library(lovepopi_patch SHARED dllmain.cpp config.hpp config.cpp vfs.hpp vfs.cpp)
target_link_libraries(lovepopi_patch "${DETOURS_LIB}")
target_link_libraries(lovepopi_patch utils)
target_link_libraries(lovepopi_patch zip)
set_target_properties(lovepopi_patch PROPERTIES OUTPUT_NAME "ドカイチャ！！ラブピカルポッピー！.deepseek-r1")

add_executable(lovepopi-chs WIN32 main.cpp winres.rc exe.manifest)
set_target_properties(lovepopi-chs PROPERTIES OUTPUT_NAME "ドカイチャ！！ラブピカルポッピー！.deepseek-r1")
