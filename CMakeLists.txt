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

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -O3 -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -nostdlib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")

add_executable(taifuse
  src/main.c
  src/cheats.c
)

#target_link_directories(taifuse BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/libs)

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
)

vita_create_self(taifuse.skprx taifuse CONFIG exports.yml UNSAFE)

vita_create_stubs(stubs taifuse ${CMAKE_CURRENT_SOURCE_DIR}/exports.yml KERNEL)

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
  COMMAND curl --ftp-method nocwd -T taifuse.skprx ftp://${PSVITAIP}:1337/ux0:/tai/taifuse.skprx && curl --ftp-method nocwd -T ../db/cheats.ini ftp://${PSVITAIP}:1337/ux0:/data/taifuse/cheats.ini
  DEPENDS taifuse.skprx
)