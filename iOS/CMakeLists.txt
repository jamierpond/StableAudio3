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
        "-framework Metal"
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
# from the Mac's download cache, which configure fills below if it is empty;
# the revision is read from Checkpoints.h so the two cannot disagree.
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

# Missing files are downloaded here, at configure time, into the same cache the
# desktop app reads. The repo is gated: the token is found the way
# Checkpoints.cpp finds it, HF_TOKEN, then $HF_HOME/token, then
# ~/.cache/huggingface/token.
set(hfToken "$ENV{HF_TOKEN}")

if (NOT hfToken AND DEFINED ENV{HF_HOME} AND EXISTS "$ENV{HF_HOME}/token")
    file(READ "$ENV{HF_HOME}/token" hfToken)
endif ()

if (NOT hfToken AND EXISTS "$ENV{HOME}/.cache/huggingface/token")
    file(READ "$ENV{HOME}/.cache/huggingface/token" hfToken)
endif ()

string(STRIP "${hfToken}" hfToken)

foreach (file IN LISTS smallFiles)
    set(target "${SA3_SMALL_CHECKPOINT_DIR}/${file}")

    if (EXISTS "${target}")
        continue()
    endif ()

    if (NOT hfToken)
        message(FATAL_ERROR
                "The iOS app ships the small checkpoint and ${target} is "
                "missing. Accept the terms on "
                "huggingface.co/stabilityai/stable-audio-3-small-music and set "
                "HF_TOKEN (or ~/.cache/huggingface/token) so configure can "
                "download it, or point SA3_SMALL_CHECKPOINT_DIR at a copy.")
    endif ()

    message(STATUS "Downloading ${file} of the small checkpoint")
    file(DOWNLOAD
            "https://huggingface.co/stabilityai/stable-audio-3-small-music/resolve/${smallRevision}/${file}"
            "${target}.part"
            HTTPHEADER "Authorization: Bearer ${hfToken}"
            SHOW_PROGRESS
            STATUS downloadStatus)
    list(GET downloadStatus 0 downloadCode)

    if (NOT downloadCode EQUAL 0)
        file(REMOVE "${target}.part")
        message(FATAL_ERROR "Downloading ${file} failed: ${downloadStatus}")
    endif ()

    file(RENAME "${target}.part" "${target}")
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
