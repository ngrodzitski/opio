# Wrapper for generating protocol entries via Cheetah: http://cheetahtemplate.org/

# Cache this script file directory path.
set(add_cheetah_generator_script_dir ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")

find_package(Python3 COMPONENTS Interpreter)
message(STATUS "Python3_EXECUTABLE = ${Python3_EXECUTABLE}")
if(${Python3_Interpreter_FOUND})
    set(cheetah_generator_script_python "${Python3_EXECUTABLE}" CACHE STRING "python interpreter to use when running Cheetah" FORCE)
    message(STATUS "Python interpreter found: ${cheetah_generator_script_python}")
else()
    find_program(cheetah_generator_script_python NAMES python3 python)
    if(cheetah_generator_script_python)
        message(STATUS "Python found: ${cheetah_generator_script_python}")
    else()
        message(FATAL_ERROR "Failed to find python")
    endif()
endif()

# proto_entry_generate_protocol_entry(
#     # The role of the entry that is get generated.
#     # Expected values are: ["server", "client"]
#     CLIENT_SERVER_ROLE <>
#
#     # For generated file a new cusom target will be created
#     # which later can be referenced as a dependency of
#     # a casual C++ target.
#     TARGET_NAME <generated_files_reference_target>
#
#     # Where the generated file will be created (usually relative to
#     # PROJECT_BINARY_DIR or to CMAKE_CURRENT_BINARY_DIR).
#     OUTPUT_DIR <path>
#
#     # Namespace for generated code.
#     OUTPUT_NAMESPACE <path>
#
#     # A variable to store generated file path (even though it is a header
#     # file it might be included into the list of source files of another
#     # cmake-target).
#     GENERATED_FILES generated_files_var
#
#     # Protocol package
#     INPUT_PACKAGE <proto.py.package>
#
#     # Additional python sys paths
#     PY_ADD_SYS_PATH <path>
# )
function (proto_entry_generate_protocol_entry)
    #TODO: Add parameter to depend on protobuf target or somehow to
    #      to detect that pb was changed.

    set(one_value_args
        CLIENT_SERVER_ROLE
        TARGET_NAME
        OUTPUT_DIR
        OUTPUT_NAMESPACE
        GENERATED_FILES
        INPUT_PACKAGE)

    set(multi_value_args PY_ADD_SYS_PATH)

    cmake_parse_arguments(cheetah "${options}"
                                  "${one_value_args}"
                                  "${multi_value_args}"
                                  ${ARGN})

    message(STATUS "[proto_entry_generate_protocol_entry] inputs: ${cheetah_INPUT_PACKAGE}")

    set(output_file ${cheetah_OUTPUT_DIR}/entry.hpp)
    set(run_cheetah ${add_cheetah_generator_script_dir}/run_cheetah.py )
    set(entry_tmpl ${add_cheetah_generator_script_dir}/templates/entry.tmpl.hpp)

    message(STATUS "[proto_entry_generate_protocol_entry] output_file: ${output_file}")
    message(STATUS "[proto_entry_generate_protocol_entry] output namespace: ${cheetah_OUTPUT_NAMESPACE}")
    message(STATUS "[proto_entry_generate_protocol_entry] run_cheetah: ${run_cheetah}")
    message(STATUS "[proto_entry_generate_protocol_entry] entry_tmpl: ${entry_tmpl}")

    add_custom_command(
        COMMAND ${cheetah_generator_script_python}
                ${run_cheetah}                  # Args for run_cheetah.py
                ${entry_tmpl}                   # 0
                "${output_file}"                # 1
                "${cheetah_OUTPUT_NAMESPACE}"   # 2
                "${cheetah_CLIENT_SERVER_ROLE}" # 3
                "${cheetah_INPUT_PACKAGE}"      # 4
                "${cheetah_PY_ADD_SYS_PATH}"    # 5 ...

        DEPENDS ${run_cheetah} ${entry_tmpl} ${cheetah_INPUT}
        VERBATIM
        OUTPUT ${output_file}
        COMMENT "Run protocol entry generation using for ${cheetah_INPUT} to ${output_file}"
    )

    message(STATUS "[proto_entry_generate_protocol_entry] add cheetah generator '${cheetah_TARGET_NAME}' DEPENDS: ${output_file}")

    add_custom_target(${cheetah_TARGET_NAME} DEPENDS ${output_file} )

    if (cheetah_GENERATED_FILES)
        set(${cheetah_GENERATED_FILES} ${output_file} PARENT_SCOPE)
    endif ()

endfunction ()
