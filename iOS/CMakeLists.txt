add_executable(StableAudio3iOS
        AudioPlayer.mm
        Backdrop.cpp
        ExpertPanel.mm
        Footprint.cpp
        Generator.cpp
        Main.cpp
        PromptField.mm
        Redraw.mm)

target_link_libraries(StableAudio3iOS PRIVATE
        eacp-sa3-pipeline
        eacp-gpu
        eacp-graphics
        "-framework AVFoundation"
        "-framework UIKit")

set(BUNDLE_ID "ai.tamber.stableaudio3")

set_target_properties(StableAudio3iOS PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "StableAudio3"
        MACOSX_BUNDLE_GUI_IDENTIFIER ${BUNDLE_ID}
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${BUNDLE_ID})

eacp_set_gui_subsystem(StableAudio3iOS)
set_default_target_setting(StableAudio3iOS)

# The small checkpoint ships inside the bundle, at the same huggingface/...
# path SA3Checkpoints::fetch looks for under the app's resources. It is copied
# from the Mac's download cache, which `StableAudio3 --model small --fetch-only`
# fills; the revision is read from Checkpoints.h so the two cannot disagree.
file(READ "${PROJECT_SOURCE_DIR}/Checkpoints.h" checkpointsHeader)
string(REGEX MATCH
        "\"stabilityai/stable-audio-3-small-music\",[ \t\r\n]*\"([0-9a-f]+)\""
        _ "${checkpointsHeader}")
set(smallRevision "${CMAKE_MATCH_1}")

if (NOT smallRevision)
    message(FATAL_ERROR "Could not read the small model's revision from Checkpoints.h")
endif ()

set(smallRepoPath
        "huggingface/stabilityai--stable-audio-3-small-music/${smallRevision}")

set(SA3_SMALL_CHECKPOINT_DIR
        "$ENV{HOME}/Library/Application Support/StableAudio3/Resources/${smallRepoPath}"
        CACHE PATH "The small checkpoint the iOS app ships")

set(smallFiles
        model.safetensors
        t5gemma-b-b-ul2/model.safetensors
        t5gemma-b-b-ul2/tokenizer.json)

foreach (file IN LISTS smallFiles)
    if (NOT EXISTS "${SA3_SMALL_CHECKPOINT_DIR}/${file}")
        message(FATAL_ERROR
                "The iOS app ships the small checkpoint, and "
                "${SA3_SMALL_CHECKPOINT_DIR}/${file} is missing. Build the "
                "desktop app and run `StableAudio3 --model small --fetch-only` "
                "on this Mac to download it, or point SA3_SMALL_CHECKPOINT_DIR "
                "at a copy.")
    endif ()
endforeach ()

# cp -c clones on APFS, so the 3.2 GB costs no time and no disk on a rebuild.
set(bundledCheckpoint "$<TARGET_BUNDLE_CONTENT_DIR:StableAudio3iOS>/${smallRepoPath}")

add_custom_command(TARGET StableAudio3iOS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${bundledCheckpoint}/t5gemma-b-b-ul2")

foreach (file IN LISTS smallFiles)
    add_custom_command(TARGET StableAudio3iOS POST_BUILD
            COMMAND /bin/cp -c -f "${SA3_SMALL_CHECKPOINT_DIR}/${file}"
                    "${bundledCheckpoint}/${file}")
endforeach ()
