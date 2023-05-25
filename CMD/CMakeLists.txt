project (PhyreConverterCMD)

set(CMD_SOURCES ${CONVERTER_SOURCES}
	"${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
)

add_executable(${PROJECT_NAME} ${CMD_SOURCES})
#add_dependencies(${PROJECT_NAME} prepare_assimp)
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${assimp_SOURCE_DIR}/contrib/zlib")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${assimp_BINARY_DIR}/contrib/zlib")

target_compile_definitions(${PROJECT_NAME} PRIVATE "CONVERTER_VERSION=\"${CONVERTER_VERSION}\"")

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${PROJECT_NAME} PUBLIC "${ADDITIONAL_INCLUDE_DIRECTORIES}")
target_link_libraries(${PROJECT_NAME} PUBLIC assimp DirectXTex)

if(POPULATE_BIN)
	set(TMP_DIR "${ROOT_PATH}/bin/tmp")
	set(CMD_BIN_NAME "FFXIIConvert")
	set(CMD_EXE_PATH "${ROOT_PATH}/bin/${CMD_BIN_NAME}.exe")
	set(CMD_ZIP_PATH "${ROOT_PATH}/bin/${CMD_BIN_NAME}v${CONVERTER_VERSION}.zip")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${TMP_DIR}")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${ROOT_PATH}/bundled_files" "${TMP_DIR}")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> "${CMD_EXE_PATH}")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMD_EXE_PATH}" "${TMP_DIR}/${CMD_BIN_NAME}.exe")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD WORKING_DIRECTORY "${ROOT_PATH}/bin" COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMD_ZIP_PATH}" --format=zip "${TMP_DIR}")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E remove "${TMP_DIR}/${CMD_BIN_NAME}.exe")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E remove_directory "${TMP_DIR}")
endif()