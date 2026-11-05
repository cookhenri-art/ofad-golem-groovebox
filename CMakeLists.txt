# Groovebox — OFAD — AGPL-3.0-or-later
# Moteur C++ / JUCE 8 + interface React unique (WebView), VST3 + application autonome.

cmake_minimum_required(VERSION 3.22)
project(Groovebox VERSION 0.47.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GROOVEBOX_BUILD_PLUGIN "Compiler le plugin et l'application autonome (nécessite JUCE)" ON)
option(GROOVEBOX_BUILD_TESTS  "Compiler les tests du moteur audio"                          ON)

# --- v0.46.1 (audit F9) : durcissement des binaires ----------------------------------------
# Windows : contrôle des appels indirects (Control Flow Guard). Linux, Mac, iPhone : protection de
# la pile et contrôles des fonctions de la bibliothèque C (version optimisée seulement).
if(MSVC)
    add_compile_options(/guard:cf)
    add_link_options(/guard:cf)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fstack-protector-strong)
    if(NOT APPLE)
        add_compile_options($<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE> $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>)
    endif()
endif()

# --- Moteur audio pur (sans JUCE) : testable seul -----------------------------
add_library(groovebox_engine STATIC
    src/engine/Engine.cpp
    src/engine/Catalog.cpp
    src/engine/Bridge.cpp)
target_include_directories(groovebox_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(groovebox_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    # Sources en UTF-8 ; optimisation du calcul flottant sans sacrifier la reproductibilité ;
    # /bigobj (v0.45.2) : le catalogue (tous les instruments et leurs banques de sons) dépasse le
    # nombre de sections d'un fichier objet ordinaire (erreur C1128)
    target_compile_options(groovebox_engine PUBLIC /utf-8 /bigobj /fp:fast /W4 /wd4100 /wd4244 /wd4267 /wd4305)
else()
    target_compile_options(groovebox_engine PRIVATE -Wall -Wextra -Wshadow -ffp-contract=fast)
endif()

# --- Encodeur MP3 (LAME 3.100, LGPL, sources dans src/third_party/lame) : v0.32 ---------------
file(GLOB GROOVEBOX_LAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/lame/unites/*.c)
if(NOT GROOVEBOX_LAME_SOURCES)
    message(FATAL_ERROR "Sources de l'encodeur MP3 introuvables : src/third_party/lame/unites/*.c")
endif()
add_library(groovebox_lame STATIC ${GROOVEBOX_LAME_SOURCES})
target_include_directories(groovebox_lame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/lame/include)
set_target_properties(groovebox_lame PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
if(MSVC)
    target_compile_options(groovebox_lame PRIVATE /w)
else()
    target_compile_options(groovebox_lame PRIVATE -w)
endif()

if(GROOVEBOX_BUILD_TESTS)
    enable_testing()
    foreach(t KickVoiceTests DrumVoicesTests SequencerTests EngineTests InstrumentTests LoudnessTests)
        add_executable(groovebox_${t} tests/${t}.cpp)
        target_link_libraries(groovebox_${t} PRIVATE groovebox_engine)
        add_test(NAME ${t} COMMAND groovebox_${t})
    endforeach()

    add_executable(groovebox_catalog tools/DumpCatalog.cpp)
    target_link_libraries(groovebox_catalog PRIVATE groovebox_engine)

    # contrôle et mise à niveau des banques d'usine (tools/presets/)
    add_executable(groovebox_presetcheck tools/PresetCheck.cpp)
    target_link_libraries(groovebox_presetcheck PRIVATE groovebox_engine)
endif()

if(NOT GROOVEBOX_BUILD_PLUGIN)
    return()
endif()

# --- JUCE 8 -----------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Chemin vers le dossier JUCE 8 (celui qui contient CMakeLists.txt et modules/)")
if(NOT JUCE_DIR OR NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_DIR introuvable. Exemple : -DJUCE_DIR=\"C:/.../JUCE/juce-8.0.12-windows/JUCE\"")
endif()
add_subdirectory(${JUCE_DIR} JUCE)

# --- Interface React compilée (ui/dist), embarquée dans le binaire ----------------
set(GROOVEBOX_UI_DIST ${CMAKE_CURRENT_SOURCE_DIR}/ui/dist)
file(GLOB_RECURSE GROOVEBOX_UI_FILES CONFIGURE_DEPENDS ${GROOVEBOX_UI_DIST}/*)
if(NOT GROOVEBOX_UI_FILES)
    message(FATAL_ERROR "Interface non compilée : dans ui/, lancer 'npm install' puis 'npm run build'.")
endif()
juce_add_binary_data(GrooveboxUiData
    HEADER_NAME UiData.h
    NAMESPACE   UiData
    SOURCES     ${GROOVEBOX_UI_FILES})

# --- Plugin VST3 + application autonome ----------------------------------------------
# v0.47 : Apple. Mac : VST3, AU et application autonome. iPhone / iPad (-G Xcode -DCMAKE_SYSTEM_NAME=iOS) :
# l'application autonome seule, en paysage, plein écran, micro et MIDI Bluetooth autorisés, fichiers
# visibles dans l'app Fichiers (dossier « Sur mon iPhone › Groovebox »).
set(GROOVEBOX_IOS OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(GROOVEBOX_IOS ON)
    set(GROOVEBOX_FORMATS Standalone)
elseif(APPLE)
    set(GROOVEBOX_FORMATS VST3 AU Standalone)
else()
    set(GROOVEBOX_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Groovebox
    COMPANY_NAME                "One For All Distribution"
    BUNDLE_ID                   OFAD.Groovebox
    PLUGIN_MANUFACTURER_CODE    Ofad
    PLUGIN_CODE                 Grvb
    FORMATS                     ${GROOVEBOX_FORMATS}
    PRODUCT_NAME                "Groovebox"
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/resources/golem/golem-icone-512.png"   # v0.46 fix6 : mascotte OFAD GOLEM
    ICON_SMALL                  "${CMAKE_CURRENT_SOURCE_DIR}/resources/golem/golem-buste-256.png"   # (petites tailles : le buste, plus lisible)
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Instrument Synth Sampler
    NEEDS_WEB_BROWSER           TRUE      # Linux : WebKitGTK
    NEEDS_WEBVIEW2              TRUE      # Windows : WebView2 lié statiquement (paquet NuGet requis)
    COPY_PLUGIN_AFTER_BUILD     FALSE
    # Apple (Mac, iPhone, iPad)
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "OFAD GOLEM records your microphone or audio input only when you open it (audio input track, sampler)."
    BLUETOOTH_PERMISSION_ENABLED TRUE
    BLUETOOTH_PERMISSION_TEXT   "OFAD GOLEM connects to Bluetooth MIDI controllers."
    BACKGROUND_AUDIO_ENABLED    TRUE
    FILE_SHARING_ENABLED        TRUE
    DOCUMENT_BROWSER_ENABLED    TRUE
    STATUS_BAR_HIDDEN           TRUE
    REQUIRES_FULL_SCREEN        TRUE
    TARGETED_DEVICE_FAMILY      "1,2"
    IPHONE_SCREEN_ORIENTATIONS  UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    IPAD_SCREEN_ORIENTATIONS    UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight)

juce_generate_juce_header(Groovebox)

target_sources(Groovebox PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/UserPresets.cpp
    src/Folders.cpp
    src/PerfStats.cpp
    src/Recorder.cpp
    src/Montage.cpp)

# v0.39 : accès au réglage « Mute audio input » de l'application autonome (pas dans le VST3)
target_sources(Groovebox_Standalone PRIVATE src/StandaloneHook.cpp)
# v0.46 fix6 : application autonome d'OFAD GOLEM (écran de lancement avec la mascotte, fenêtre
# ouverte agrandie) à la place de celle de JUCE : voir src/StandaloneHook.cpp
# (v0.47 : pas sur iPhone / iPad, qui gardent l'application de JUCE, comme Android)
if(NOT GROOVEBOX_IOS)
    target_compile_definitions(Groovebox_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
endif()
target_link_libraries(Groovebox_Standalone PRIVATE GrooveboxUiData)

target_compile_definitions(Groovebox PUBLIC
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>)

target_link_libraries(Groovebox
    PRIVATE
        GrooveboxUiData
        groovebox_engine
        groovebox_lame
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Outil de vérification : rendu hors temps réel du processeur dans un WAV ----------------
if(GROOVEBOX_IOS)
    return() # pas d'outil en ligne de commande sur iPhone / iPad
endif()
juce_add_console_app(groovebox_render PRODUCT_NAME "groovebox_render")
target_sources(groovebox_render PRIVATE tools/RenderDemo.cpp src/PluginProcessor.cpp src/Recorder.cpp src/Montage.cpp)
target_compile_definitions(groovebox_render PRIVATE
    JucePlugin_Name="Groovebox"
    GROOVEBOX_NO_EDITOR=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
juce_generate_juce_header(groovebox_render)
target_link_libraries(groovebox_render PRIVATE
    groovebox_engine
    groovebox_lame
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags)

# --- v0.46.1 : tests de sécurité (audit du 4 novembre 2026) : projets piégés, montage, fichiers abîmés ----
if(GROOVEBOX_BUILD_TESTS)
    juce_add_console_app(groovebox_SecurityTests PRODUCT_NAME "groovebox_SecurityTests")
    target_sources(groovebox_SecurityTests PRIVATE tools/SecurityTests.cpp src/PluginProcessor.cpp src/Recorder.cpp src/Montage.cpp)
    target_compile_definitions(groovebox_SecurityTests PRIVATE
        JucePlugin_Name="Groovebox"
        GROOVEBOX_NO_EDITOR=1
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    juce_generate_juce_header(groovebox_SecurityTests)
    target_link_libraries(groovebox_SecurityTests PRIVATE
        groovebox_engine
        groovebox_lame
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    add_test(NAME SecurityTests COMMAND groovebox_SecurityTests)
endif()
