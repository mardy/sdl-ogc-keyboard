function(add_resource out_var)
    set(result)
    foreach(in_f ${ARGN})
        string(MAKE_C_IDENTIFIER ${in_f} input_identifier)
        set(out_f "${CMAKE_CURRENT_BINARY_DIR}/${input_identifier}.o")

        add_custom_command(
            OUTPUT ${out_f}
            COMMAND ${CMAKE_LINKER} --relocatable --format binary --output ${out_f} ${in_f}
            DEPENDS ${in_f}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Embedding ${in_f} as a resource"
        )
        list(APPEND result ${out_f})
    endforeach()
    set(${out_var} ${result} PARENT_SCOPE)
endfunction()
