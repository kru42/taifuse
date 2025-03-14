cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(danganronpa_plugin)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

add_executable(danganronpa_plugin
  src/main.c
)

target_link_libraries(danganronpa_plugin
  taipool
  taihen_stub
  k
  gcc
  SceLibKernel_stub
  SceLibc_stub
  SceNet_stub
  SceKernelModulemgr_stub 
  SceNetCtl_stub
  SceSysmodule_stub
  SceKernelThreadMgr_stub
  SceIofilemgr_stub
  SceRtc_stub
  SceAppMgr_stub
  ScePower_stub
)

vita_create_self(danganronpa_plugin.suprx danganronpa_plugin CONFIG exports.yml UNSAFE)

#vita_create_stubs(stubs soul_sacrifice_plugin ${CMAKE_CURRENT_SOURCE_DIR}/exports.yml KERNEL)

# add_custom_target(taifuse_all
#   ALL
#   DEPENDS libtaifuse_stub.a
#   DEPENDS libtaifuse_stub_weak.a
# )

# install(DIRECTORY ${CMAKE_BINARY_DIR}/stubs/
#   DESTINATION lib
#   FILES_MATCHING PATTERN "*.a"
# )

# install(FILES taifuse.h
#   DESTINATION include
# )

set(PSVITAIP 192.168.1.58)

add_custom_target(send
  COMMAND curl --ftp-method nocwd -T danganronpa_plugin.suprx ftp://${PSVITAIP}:1337/ux0:/data/taifuse/plugins/PCSE00261/danganronpa_plugin.suprx
  DEPENDS danganronpa_plugin.suprx
)
