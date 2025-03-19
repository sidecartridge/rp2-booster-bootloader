# Create a library for the MD5 files
add_library(md5
    md5.c
)

# Add the include directory (for md5.h)
target_include_directories(md5 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link necessary libraries
target_link_libraries(md5
    pico_stdlib    # Core Pico SDK library
)
