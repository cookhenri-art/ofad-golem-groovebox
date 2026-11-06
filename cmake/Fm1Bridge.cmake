# Groovebox — OFAD — AGPL-3.0-or-later
#
# v0.49 : lecteur de firmwares FM-1 (« FM Player »). L'émulateur est écrit en Rust (third_party/fm1emu,
# fm1-emu de Joakim Eriksson, licence MIT) : cargo le compile en bibliothèque statique, liée au moteur.
# Rust ne sert qu'à compiler (rien à installer chez l'utilisateur). Sans cargo, GOLEM se compile quand
# même : l'instrument est présent mais dit « lecteur indisponible dans cette version ».
#
# Résultat : la variable GROOVEBOX_FM1 (ON / OFF) et, si ON, la cible importée fm1_bridge.

option(GROOVEBOX_FM1 "Lecteur de firmwares FM-1 (demande Rust / cargo pour compiler)" ON)

if(GROOVEBOX_FM1)
    find_program(GROOVEBOX_CARGO cargo HINTS "$ENV{CARGO_HOME}/bin" "$ENV{HOME}/.cargo/bin" "$ENV{USERPROFILE}/.cargo/bin")
    if(NOT GROOVEBOX_CARGO)
        message(WARNING "cargo (Rust) introuvable : GOLEM est compilé sans le lecteur de firmwares FM-1. "
                        "Installez Rust (https://rustup.rs) puis relancez CMake pour l'avoir.")
        set(GROOVEBOX_FM1 OFF)
    endif()
endif()

if(NOT GROOVEBOX_FM1)
    return()
endif()

set(FM1_SRC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fm1emu)
set(FM1_OUT ${CMAKE_BINARY_DIR}/fm1emu)
file(GLOB_RECURSE FM1_RS CONFIGURE_DEPENDS ${FM1_SRC}/crates/*.rs ${FM1_SRC}/crates/*/Cargo.toml)
list(APPEND FM1_RS ${FM1_SRC}/Cargo.toml ${FM1_SRC}/Cargo.lock)

if(WIN32)
    set(FM1_LIBNAME fm1_bridge.lib)
else()
    set(FM1_LIBNAME libfm1_bridge.a)
endif()

# Cible(s) Rust selon la plateforme visée (sans --target : la machine qui compile)
set(FM1_TRIPLES "")
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(FM1_TRIPLES aarch64-apple-ios)
elseif(APPLE)
    set(_archs ${CMAKE_OSX_ARCHITECTURES})
    if(NOT _archs)
        set(_archs ${CMAKE_HOST_SYSTEM_PROCESSOR})
    endif()
    foreach(a ${_archs})
        if(a STREQUAL "arm64")
            list(APPEND FM1_TRIPLES aarch64-apple-darwin)
        else()
            list(APPEND FM1_TRIPLES x86_64-apple-darwin)
        endif()
    endforeach()
elseif(MSVC)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(FM1_TRIPLES aarch64-pc-windows-msvc)
    else()
        set(FM1_TRIPLES x86_64-pc-windows-msvc)
    endif()
endif()

set(FM1_LIBS "")
set(FM1_CMDS "")
# GOLEM 1.0 : messages de cargo sur une ligne (--message-format short) : Visual Studio prenait le mot
# « error » d'un extrait de code cité dans un simple avertissement pour une erreur, et arrêtait la fabrication.
if(FM1_TRIPLES)
    foreach(t ${FM1_TRIPLES})
        list(APPEND FM1_LIBS ${FM1_OUT}/${t}/release/${FM1_LIBNAME})
        list(APPEND FM1_CMDS COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${FM1_OUT} CARGO_TERM_COLOR=never
             ${GROOVEBOX_CARGO} build --release --locked --message-format short -p fm1-bridge --target ${t})
    endforeach()
else()
    list(APPEND FM1_LIBS ${FM1_OUT}/release/${FM1_LIBNAME})
    list(APPEND FM1_CMDS COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${FM1_OUT} CARGO_TERM_COLOR=never
         ${GROOVEBOX_CARGO} build --release --locked --message-format short -p fm1-bridge)
endif()

list(LENGTH FM1_LIBS _n)
if(_n GREATER 1)
    # Mac universel : une bibliothèque par architecture, réunies par lipo
    set(FM1_LIB ${FM1_OUT}/universal/${FM1_LIBNAME})
    list(APPEND FM1_CMDS COMMAND ${CMAKE_COMMAND} -E make_directory ${FM1_OUT}/universal
         COMMAND lipo -create ${FM1_LIBS} -output ${FM1_LIB})
else()
    list(GET FM1_LIBS 0 FM1_LIB)
endif()

add_custom_command(OUTPUT ${FM1_LIB}
    ${FM1_CMDS}
    WORKING_DIRECTORY ${FM1_SRC}
    DEPENDS ${FM1_RS}
    COMMENT "Émulateur FM-1 (Rust, cargo build --release)"
    VERBATIM)
add_custom_target(fm1_bridge_build DEPENDS ${FM1_LIB})

add_library(fm1_bridge STATIC IMPORTED GLOBAL)
set_target_properties(fm1_bridge PROPERTIES
    IMPORTED_LOCATION ${FM1_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${FM1_SRC}/include)
add_dependencies(fm1_bridge fm1_bridge_build)

# Bibliothèques système dont la bibliothèque standard de Rust a besoin
if(WIN32)
    set_property(TARGET fm1_bridge APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32 userenv ntdll bcrypt advapi32 kernel32)
elseif(APPLE)
    set_property(TARGET fm1_bridge APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation" "-framework Security")
else()
    find_package(Threads REQUIRED)
    set_property(TARGET fm1_bridge APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
