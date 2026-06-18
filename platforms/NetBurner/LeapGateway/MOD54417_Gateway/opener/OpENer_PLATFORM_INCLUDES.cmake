macro(opener_platform_spec)
  include_directories(${PORTS_SRC_DIR}/${OpENer_PLATFORM} ${PORTS_SRC_DIR}/${OpENer_PLATFORM}/leap_gateway)
  if(DEFINED ENV{NNDK_ROOT})
    include_directories(
      "$ENV{NNDK_ROOT}/nbrtos/include"
      "$ENV{NNDK_ROOT}/platform/MOD5441X/include"
      "$ENV{NNDK_ROOT}/arch/coldfire/include"
      "$ENV{NNDK_ROOT}/arch/coldfire/cpu/MCF5441X/include"
      "$ENV{NNDK_ROOT}/libraries/include")
  else()
    include_directories(
      "C:/nburn/nbrtos/include"
      "C:/nburn/platform/MOD5441X/include"
      "C:/nburn/arch/coldfire/include"
      "C:/nburn/arch/coldfire/cpu/MCF5441X/include"
      "C:/nburn/libraries/include")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fcommon -mcpu=54415 -fno-builtin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -mcpu=54415 -fno-builtin -fno-exceptions -fno-rtti")
  add_definitions(-DOPENER_NETBURNER)
endmacro(opener_platform_spec)
