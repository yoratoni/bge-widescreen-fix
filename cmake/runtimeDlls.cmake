# Copies the runtime DLLs of a target next to its executable after each build (Windows only).
# Usage: copy_runtime_dlls(<target>)
#
# Note: "copy_if_different" fails when "TARGET_RUNTIME_DLLS" is empty (no shared dependencies),
# so the whole command is wrapped in a "BOOL" generator expression and collapses to nothing
function(copy_runtime_dlls target)
    if(NOT WIN32)
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:${target}>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:${target}>;$<TARGET_FILE_DIR:${target}>>"
        COMMAND_EXPAND_LISTS
    )
endfunction()
