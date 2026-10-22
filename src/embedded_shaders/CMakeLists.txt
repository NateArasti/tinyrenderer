cmake_minimum_required(VERSION 4.3.1)

find_program(SLANGC_EXECUTABLE slangc REQUIRED)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(GENERATOR_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/GenerateEmbeddedShaderCode.cmake")

function(add_embedded_shader_code shader_name class_name header_file)
    set(slang_file "${SHADER_SOURCE_DIR}/${shader_name}.slang")
    set(binary_file "${GENERATED_DIR}/${shader_name}.spv")
    set(code_file "${GENERATED_DIR}/${shader_name}_code.cpp")

    add_custom_command(
        OUTPUT "${code_file}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DIR}"
        COMMAND
            "${CMAKE_COMMAND}"
            "-DSLANGC_EXECUTABLE=${SLANGC_EXECUTABLE}"
            "-DINPUT_SOURCE=${shader_name}.slang"
            "-DWORKING_DIRECTORY=${SHADER_SOURCE_DIR}"
            "-DOUTPUT_BINARY=${binary_file}"
            "-DOUTPUT_SOURCE=${code_file}"
            "-DHEADER_FILE=${header_file}"
            "-DCLASS_NAME=${class_name}"
            "-DTARGET_PROFILE=spirv"
            -P "${GENERATOR_SCRIPT}"
        DEPENDS "${slang_file}" ${ARGN} "${GENERATOR_SCRIPT}" "${CMAKE_CURRENT_SOURCE_DIR}/${header_file}"
        VERBATIM
    )

    set(EMBEDDED_SHADER_CODE_FILES ${EMBEDDED_SHADER_CODE_FILES} "${code_file}" PARENT_SCOPE)
endfunction()

add_embedded_shader_code(
    pbr 
    Pbr 
    pbr.h 

    "${SHADER_SOURCE_DIR}/light.slang"
    "${SHADER_SOURCE_DIR}/environment.slang"
    "${SHADER_SOURCE_DIR}/shared_structs.slang"
    "${SHADER_SOURCE_DIR}/math.slang"
)
add_embedded_shader_code(
    shadow Shadow shadow.h

    "${SHADER_SOURCE_DIR}/shared_structs.slang"
)
add_embedded_shader_code(skybox Skybox skybox.h 
    "${SHADER_SOURCE_DIR}/environment.slang"
    "${SHADER_SOURCE_DIR}/shared_structs.slang"
)
add_embedded_shader_code(
    equirectangular_conversion
    EquirectangularConversion
    equirectangular_conversion.h 

    "${SHADER_SOURCE_DIR}/environment.slang"
    "${SHADER_SOURCE_DIR}/math.slang"
)
add_embedded_shader_code(
    irradiance_convolution
    IrradianceConvolution
    irradiance_convolution.h
    
    "${SHADER_SOURCE_DIR}/environment.slang"
    "${SHADER_SOURCE_DIR}/math.slang"
)
add_embedded_shader_code(
    prefilter_environment
    PrefilterEnvironment
    prefilter_environment.h

    "${SHADER_SOURCE_DIR}/environment.slang"
    "${SHADER_SOURCE_DIR}/ibl.slang"
    "${SHADER_SOURCE_DIR}/light.slang"
    "${SHADER_SOURCE_DIR}/math.slang"
)
add_embedded_shader_code(
    brdf_integration
    BRDFIntegration
    brdf_integration.h

    "${SHADER_SOURCE_DIR}/ibl.slang"
)
add_embedded_shader_code(
    post_processing
    PostProcessing
    post_processing.h
)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
)
add_library(embedded_shaders STATIC ${SOURCES} ${EMBEDDED_SHADER_CODE_FILES})

target_include_directories(embedded_shaders PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(embedded_shaders PUBLIC data)
