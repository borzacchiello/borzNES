cmake_minimum_required ( VERSION 3.19 )

project ( borzNES )

option ( ASAN "Compile with asan (only for debug builds)" OFF )

set ( CMAKE_C_STANDARD 17 )

set ( borzNES_src
    6502_cpu.c
    apu.c
    alloc.c
    cartridge.c
    logging.c
    mapper.c
    memory.c
    ppu.c
    system.c
    window.c
    game_window.c )

set ( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall" )
set ( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -Wall" )

if ( ASAN )
    set ( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address,undefined" )
    set ( CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined" )
endif ()

add_executable ( borznes
    ${borzNES_src}
    main.c )

add_executable ( borznes_multi
    ${borzNES_src}
    multiplayer_main.c )

target_link_libraries ( borznes LINK_PUBLIC SDL2 SDL2_ttf )
target_link_libraries ( borznes_multi LINK_PUBLIC SDL2 SDL2_ttf )

add_custom_command (
        TARGET borznes POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/../font/courier.ttf
                ${CMAKE_CURRENT_BINARY_DIR}/courier.ttf )
