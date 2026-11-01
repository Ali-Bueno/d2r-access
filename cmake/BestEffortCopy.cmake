# Usage: cmake -DSOURCE=<file> -DDEST_DIR=<folder> -P BestEffortCopy.cmake
# Copies when the folder exists; a missing folder or a locked file only warns.
if(NOT IS_DIRECTORY "${DEST_DIR}")
	message(STATUS "Not copied, folder missing: ${DEST_DIR}")
	return()
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SOURCE}" "${DEST_DIR}"
	RESULT_VARIABLE result ERROR_QUIET)
if(NOT result EQUAL 0)
	message(WARNING "Could not copy ${SOURCE} to ${DEST_DIR} (is it running?)")
endif()
