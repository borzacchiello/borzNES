cmake_minimum_required ( VERSION 3.19 )

project ( borzNES )

option ( ASAN "Compile with asan (only for debug builds)" OFF )
option ( WIN  "Cross-compile for Windows" OFF )
option ( GWDEBUG  "Enable rich debug game window" OFF )

set ( WIN_SDL2 "" CACHE STRING "Path to sdl2 mingw" )
set ( CIFUZZ $ENV{CIFUZZ} )

set ( CMAKE_C_STANDARD 17 )

set ( mappers_src
    mappers/mapper_common.c
    mappers/000_nrom.c
    mappers/001_mmc1.c
    mappers/003_cnrom.c
    mappers/004_mmc3.c
    mappers/005_mmc5.c
    mappers/007_axrom.c
    mappers/009_mmc2.c
    mappers/010_mmc4.c
    mappers/071_camerica.c
    mappers/163_fc001.c )

set ( borzNES_src
    6502_cpu.c
    apu.c
    alloc.c
    cartridge.c
    mapper.c
    memory.c
    ppu.c
    system.c
    window.c
    game_window.c
    input_handler.c
    config.c
    ${mappers_src} )

set ( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall" )
set ( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -Wall" )

if ( CIFUZZ )
    enable_testing ()

    find_package ( cifuzz NO_SYSTEM_ENVIRONMENT_PATH )
    enable_fuzz_testing ()

    add_compile_definitions ( NOLEAK=1 )
endif ()

if ( GWDEBUG )
    set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_DEBUG_GW" )
endif ()

if ( ASAN )
    set ( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address,undefined" )
    set ( CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined" )
endif ()

if ( WIN )
    if ( WIN_SDL2 STREQUAL "" )
        message ( FATAL_ERROR "Missing SDL2 path (-DWIN_SDL2=/path/to/win_sdl2/x86_64-w64-mingw32/)" )
    endif ()

    set ( CMAKE_C_COMPILER "x86_64-w64-mingw32-gcc" )
    set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSDL_MAIN_HANDLED" )
    include_directories ( ${WIN_SDL2}/include )
    link_directories ( ${WIN_SDL2}/lib )
endif ()

add_executable ( borznes_multi
    ${borzNES_src}
    logging.c
    async.c
    tools/borznes_multi.c )

add_executable ( borznes
    ${borzNES_src}
    logging.c
    async.c
    tools/borznes.c )

add_executable ( rom_info
    alloc.c
    logging.c
    cartridge.c
    tools/rom_info.c )

add_executable ( define_keys
    ${borzNES_src}
    logging.c
    tools/define_keys.c )

target_link_libraries ( borznes LINK_PUBLIC SDL2 SDL2_ttf )
target_link_libraries ( borznes_multi LINK_PUBLIC SDL2 SDL2_ttf )
target_link_libraries ( define_keys LINK_PUBLIC SDL2 SDL2_ttf )

if ( WIN )
    target_link_libraries ( borznes_multi LINK_PUBLIC ws2_32 pthread )
    target_link_libraries ( borznes LINK_PUBLIC ws2_32 pthread )
endif ()

add_custom_command (
    TARGET borznes POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/../resources/courier.ttf
            ${CMAKE_CURRENT_BINARY_DIR}/courier.ttf )

add_custom_command (
    TARGET borznes POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/../resources/borznes_cfg.txt
            ${CMAKE_CURRENT_BINARY_DIR}/borznes_cfg.txt )

if ( WIN )
    add_custom_command (
        TARGET borznes POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${WIN_SDL2}/bin/SDL2.dll
                ${CMAKE_CURRENT_BINARY_DIR}/SDL2.dll )

    add_custom_command (
        TARGET borznes POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${WIN_SDL2}/bin/SDL2_ttf.dll
                ${CMAKE_CURRENT_BINARY_DIR}/SDL2_ttf.dll )

    add_custom_command (
        TARGET borznes_multi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll
                ${CMAKE_CURRENT_BINARY_DIR}/libwinpthread-1.dll )
endif ()

if ( CIFUZZ )
    add_fuzz_test ( system_fuzz_test
        ${borzNES_src}
        tests/system_fuzz_test.c )

    target_link_libraries ( system_fuzz_test LINK_PUBLIC SDL2 SDL2_ttf )

    add_fuzz_test ( cpu_fuzz_test
        ${borzNES_src}
        tests/cpu_fuzz_test.c )

    target_link_libraries ( cpu_fuzz_test LINK_PUBLIC SDL2 SDL2_ttf )

    add_fuzz_test ( load_state_fuzz_test
        ${borzNES_src}
        tests/load_state_fuzz_test.c )

    target_link_libraries ( load_state_fuzz_test LINK_PUBLIC SDL2 SDL2_ttf )

endif ()
