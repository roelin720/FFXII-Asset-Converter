project (ConverterGUI)

include(GLFW)
include(ImGUI)
include(CTPL)	
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${imgui_SOURCE_DIR}")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${glfw_SOURCE_DIR}")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${ctpl_SOURCE_DIR}")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${assimp_SOURCE_DIR}/contrib/zlib")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${assimp_BINARY_DIR}/contrib/zlib")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${THIRD_PARTY_DIR}/glad/include")

set(GUI_SOURCES ${CONVERTER_SOURCES} ${IMGUI_SOURCES} ${icon_headers}
	"${THIRD_PARTY_DIR}/glad/include/glad/glad.h"
	"${THIRD_PARTY_DIR}/glad/src/glad.c"
	"${CMAKE_CURRENT_LIST_DIR}/VBFClipHelper.h"
	"${CMAKE_CURRENT_LIST_DIR}/VBFClipHelper.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ClipDataObject.h"
	"${CMAKE_CURRENT_LIST_DIR}/ClipDataObject.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ClipDropSource.h"
	"${CMAKE_CURRENT_LIST_DIR}/ClipDropSource.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ClipDropTarget.h"
	"${CMAKE_CURRENT_LIST_DIR}/ClipDropTarget.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ClipDragDrop.h"
	"${CMAKE_CURRENT_LIST_DIR}/ClipDragDrop.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/GUILogs.h"
	"${CMAKE_CURRENT_LIST_DIR}/GUILogs.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/GUIPathHistory.h"
	"${CMAKE_CURRENT_LIST_DIR}/GUIPathHistory.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/GUIIcon.h"
	"${CMAKE_CURRENT_LIST_DIR}/GUIIcon.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Process.h"
	"${CMAKE_CURRENT_LIST_DIR}/Process.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Pipe.h"
	"${CMAKE_CURRENT_LIST_DIR}/Pipe.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Audio.h"
	"${CMAKE_CURRENT_LIST_DIR}/Audio.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/GUI.h"
	"${CMAKE_CURRENT_LIST_DIR}/GUI.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/GUIBrowser.h"
	"${CMAKE_CURRENT_LIST_DIR}/GUIBrowser.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/file_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/folder_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/file_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/folder_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/archive_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/image_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/model_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/music_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/text_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/video_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/exe_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/dll_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/pin_icon_small_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/left_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/right_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/up_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/down_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/left_icon_tiny_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/right_icon_tiny_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/play_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/play_file_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/play_muted_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/play_unmuted_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/copy_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/visibility_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/save_icon_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/app_icon16_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/app_icon32_data.h"
	"${CMAKE_CURRENT_LIST_DIR}/Icons/app_icon48_data.h"
)

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE )
if (BUILD_TYPE STREQUAL "debug")
    message("debug mode")
	add_executable(${PROJECT_NAME} ${GUI_SOURCES} "${CMAKE_CURRENT_LIST_DIR}/app_icon.rc")
else()
    message("release mode")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
	add_executable(${PROJECT_NAME} WIN32 ${GUI_SOURCES} "${CMAKE_CURRENT_LIST_DIR}/app_icon.rc")
endif()
add_dependencies(${PROJECT_NAME} GenerateEnums)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

target_compile_definitions(${PROJECT_NAME} PRIVATE "CONVERTER_VERSION=\"${CONVERTER_VERSION}\"")
target_compile_definitions(${PROJECT_NAME} PRIVATE "GUI_BUILD")
if (BUILD_TYPE STREQUAL "debug")
target_compile_definitions(${PROJECT_NAME} PRIVATE "DEBUG")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${PROJECT_NAME} PUBLIC "${ADDITIONAL_INCLUDE_DIRECTORIES}")
target_link_libraries(${PROJECT_NAME} PUBLIC assimp DirectXTex glfw shlwapi comctl32)

if(POPULATE_BIN)
	set(GUI_BIN_NAME "FFXIIConvertGUI")
	set(TMP_DIR "${ROOT_PATH}/bin/${GUI_BIN_NAME}v${CONVERTER_VERSION}")
	set(GUI_EXE_PATH "${ROOT_PATH}/bin/${GUI_BIN_NAME}.exe")
	set(GUI_ZIP_PATH "${ROOT_PATH}/bin/${GUI_BIN_NAME}v${CONVERTER_VERSION}.zip")	
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${TMP_DIR}")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${ROOT_PATH}/bundled_files" "${TMP_DIR}")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${GUI_EXE_PATH}")
	#add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_RC_COMPILER} "${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc" "${GUI_EXE_PATH}")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${GUI_EXE_PATH}" "${TMP_DIR}/${GUI_BIN_NAME}.exe")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD WORKING_DIRECTORY "${ROOT_PATH}/bin" COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${GUI_ZIP_PATH}" --format=zip "${TMP_DIR}")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E remove "${TMP_DIR}/${GUI_BIN_NAME}.exe")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E remove_directory "${TMP_DIR}")
endif()