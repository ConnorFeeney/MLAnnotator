# Collect all source files
file(GLOB_RECURSE SRC_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.c"
)

# Create executable from source files
add_executable(cvClient ${SRC_FILES})

# Add include directories
target_include_directories(cvClient PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
