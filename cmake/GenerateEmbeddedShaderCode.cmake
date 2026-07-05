if(NOT DEFINED SLANGC_EXECUTABLE)
    message(FATAL_ERROR "SLANGC_EXECUTABLE is required")
endif()

if(NOT DEFINED INPUT_SOURCE)
    message(FATAL_ERROR "INPUT_SOURCE is required")
endif()

if(NOT DEFINED WORKING_DIRECTORY)
    message(FATAL_ERROR "WORKING_DIRECTORY is required")
endif()

if(NOT DEFINED OUTPUT_BINARY)
    message(FATAL_ERROR "OUTPUT_BINARY is required")
endif()

if(NOT DEFINED OUTPUT_SOURCE)
    message(FATAL_ERROR "OUTPUT_SOURCE is required")
endif()

if(NOT DEFINED HEADER_FILE)
    message(FATAL_ERROR "HEADER_FILE is required")
endif()

if(NOT DEFINED CLASS_NAME)
    message(FATAL_ERROR "CLASS_NAME is required")
endif()

if(NOT DEFINED TARGET_PROFILE)
    set(TARGET_PROFILE spirv)
endif()

get_filename_component(output_binary_abs "${OUTPUT_BINARY}" ABSOLUTE)
get_filename_component(output_source_abs "${OUTPUT_SOURCE}" ABSOLUTE)
get_filename_component(output_binary_dir "${output_binary_abs}" DIRECTORY)
get_filename_component(output_source_dir "${output_source_abs}" DIRECTORY)
file(MAKE_DIRECTORY "${output_binary_dir}" "${output_source_dir}")

execute_process(
    COMMAND "${SLANGC_EXECUTABLE}" "${INPUT_SOURCE}" -target "${TARGET_PROFILE}" -o "${output_binary_abs}"
    WORKING_DIRECTORY "${WORKING_DIRECTORY}"
    RESULT_VARIABLE slang_result
)

if(NOT slang_result EQUAL 0)
    message(FATAL_ERROR "Failed to compile ${INPUT_SOURCE}")
endif()

file(READ "${output_binary_abs}" shader_hex HEX)
string(LENGTH "${shader_hex}" hex_length)

set(output "#include \"${HEADER_FILE}\"\n\nnamespace tr::Data::EmbeddedShaders {\n    std::vector<char> ${CLASS_NAME}::getCode() {\n        static constexpr unsigned char code[] = {\n")

math(EXPR byte_count "${hex_length} / 2")
if(byte_count GREATER 0)
    math(EXPR last_byte "${byte_count} - 1")
    foreach(byte_index RANGE 0 ${last_byte})
        math(EXPR offset "${byte_index} * 2")
        string(SUBSTRING "${shader_hex}" ${offset} 2 byte)
        string(APPEND output "            0x${byte},\n")
    endforeach()
endif()

string(APPEND output "        };\n        const auto* begin = reinterpret_cast<const char*>(code);\n        return { begin, begin + sizeof(code) };\n    }\n}\n")
file(WRITE "${output_source_abs}" "${output}")
