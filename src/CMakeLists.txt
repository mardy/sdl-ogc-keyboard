find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL REQUIRED IMPORTED_TARGET sdl2)


add_library(OskCommon STATIC
    config.c
    config.h
)

target_include_directories(OskCommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(CMAKE_CROSSCOMPILING)
    set(TARGET sdl-ogcosk)

    set(SOURCES
        keyboard.c
        ogc_keyboard.h
    )

    add_library(${TARGET} STATIC ${SOURCES})

    target_include_directories(${TARGET} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${TARGET} PUBLIC
        PkgConfig::SDL
        OskCommon
    )
endif()
