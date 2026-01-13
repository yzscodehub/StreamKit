# phoenix/model - Data model library (Project, Sequence, Track, Clip)

add_library(phoenix_model
    src/project_io.cpp
)

target_include_directories(phoenix_model
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(phoenix_model
    PUBLIC
        phoenix::core
    PRIVATE
        nlohmann_json::nlohmann_json
)

target_compile_features(phoenix_model PUBLIC cxx_std_20)

# Alias for convenient linking
add_library(phoenix::model ALIAS phoenix_model)
