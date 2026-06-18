opener_common_includes()
opener_platform_support("INCLUDES")

add_library(LeapGateway leapgateway.c ethlinkcbs.c)
target_link_libraries(LeapGateway NVDATA)
