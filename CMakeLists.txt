cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(taifuse)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

add_executable(taifuse
  src/main.c
  src/taifuse.c
  src/utils.c
  src/cheats.c
  src/gui.c
  src/plugin.c
  src/menu.c
  src/console.c
  src/hex_browser.c
)

target_link_libraries(taifuse
  taihenForKernel_stub
  taihenModuleUtils_stub
  SceSysmemForDriver_stub
  SceSysmemForKernel_stub
  SceSysclibForDriver_stub
  SceSysrootForKernel_stub
  SceThreadmgrForDriver_stub
  SceModulemgrForKernel_stub
  SceModulemgrForDriver_stub
  SceIofilemgrForDriver_stub
  SceDebugForDriver_stub
  SceCtrlForDriver_stub
  SceDisplayForDriver_stub
)

vita_create_self(taifuse.skprx taifuse CONFIG exports.yml UNSAFE)

# Create stubs for userland modules to use
vita_create_stubs(taifuse_stubs taifuse ${CMAKE_CURRENT_SOURCE_DIR}/exports.yml KERNEL)

# Install stubs to the build directory so other projects can find them
add_custom_target(install_stubs
  ALL
  COMMAND mkdir -p ${CMAKE_BINARY_DIR}/stubs
  COMMAND cp libtaifuse_stub.a ${CMAKE_BINARY_DIR}/stubs/
  COMMAND cp libtaifuse_stub_weak.a ${CMAKE_BINARY_DIR}/stubs/
  DEPENDS taifuse.skprx
)

set(PSVITAIP 192.168.1.58)

add_custom_target(send
  COMMAND curl --ftp-method nocwd -T taifuse.skprx ftp://${PSVITAIP}:1337/ux0:/tai/taifuse.skprx && curl --ftp-method nocwd -T ../db/cheats.ini ftp://${PSVITAIP}:1337/ux0:/data/taifuse/cheats.ini
  DEPENDS taifuse.skprx
)