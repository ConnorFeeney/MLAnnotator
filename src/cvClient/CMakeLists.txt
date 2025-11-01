# Collect all source files
file(GLOB_RECURSE SRC_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# Create executable from source files
add_executable(cvClient ${SRC_FILES})

# Add include directories
target_include_directories(cvClient PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
