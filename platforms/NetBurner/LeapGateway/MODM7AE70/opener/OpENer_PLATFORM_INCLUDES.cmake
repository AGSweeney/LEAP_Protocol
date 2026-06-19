macro(opener_platform_spec)
  include_directories(${PORTS_SRC_DIR}/${OpENer_PLATFORM} ${PORTS_SRC_DIR}/${OpENer_PLATFORM}/leap_gateway)
  if(DEFINED ENV{NNDK_ROOT})
    include_directories(
      "$ENV{NNDK_ROOT}/nbrtos/include"
      "$ENV{NNDK_ROOT}/platform/MODM7AE70/include"
      "$ENV{NNDK_ROOT}/arch/cortex-m7/include"
      "$ENV{NNDK_ROOT}/arch/cortex-m7/cpu/SAME70/include"
      "$ENV{NNDK_ROOT}/libraries/include")
  else()
    include_directories(
      "C:/nburn/nbrtos/include"
      "C:/nburn/platform/MODM7AE70/include"
      "C:/nburn/arch/cortex-m7/include"
      "C:/nburn/arch/cortex-m7/cpu/SAME70/include"
      "C:/nburn/libraries/include")
  endif()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -fcommon -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=softfp -mthumb -fno-builtin")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=softfp -mthumb -fno-builtin -fno-exceptions -fno-rtti")
  add_definitions(-DOPENER_NETBURNER)
endmacro(opener_platform_spec)
