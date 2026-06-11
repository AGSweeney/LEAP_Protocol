opener_common_includes()
opener_platform_support("INCLUDES")

add_library(SAMPLE_APP sampleapplication.c)
target_link_libraries(SAMPLE_APP NVDATA)
