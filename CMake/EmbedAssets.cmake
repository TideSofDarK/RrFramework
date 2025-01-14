file(
    WRITE "${ASSET_HEADER_PATH}"
    "#ifndef RC_INVOKED\n"
    "#include <Rr/Rr_Asset.h>\n"
    "#endif\n"
    "\n"
)
if(RR_USE_RC)
    foreach (ASSET_PATH ${ASSETS_LIST})
        cmake_path(ABSOLUTE_PATH ASSET_PATH NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

        get_filename_component(ASSET_NAME ${ASSET_PATH} NAME)
        string(MAKE_C_IDENTIFIER "${ASSET_NAME}" ASSET_IDENTIFIER)
        string(TOUPPER "${ASSET_IDENTIFIER}" ASSET_IDENTIFIER)
        set(ASSET_FINAL_IDENTIFIER "${IDENTIFIER_PREFIX}${ASSET_IDENTIFIER}")
        file(
            APPEND "${ASSET_HEADER_PATH}"
            "#define ${ASSET_FINAL_IDENTIFIER} \"${ASSET_FINAL_IDENTIFIER}_ID\"\n"
            "#if defined(RC_INVOKED)\n"
            "${ASSET_FINAL_IDENTIFIER}_ID RRDATA \"${AssetAbsolutePath}\"\n"
            "#endif\n"
            "\n"
        )
    endforeach (ASSET_PATH)
else()
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#if defined(RR_DEFINE_ASSETS)\n"
    )
    foreach (ASSET_PATH ${ASSETS_LIST})
        cmake_path(ABSOLUTE_PATH ASSET_PATH NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

        get_filename_component(ASSET_NAME ${ASSET_PATH} NAME)
        string(MAKE_C_IDENTIFIER "${ASSET_NAME}" ASSET_IDENTIFIER)
        string(TOUPPER "${ASSET_IDENTIFIER}" ASSET_IDENTIFIER)
        set(ASSET_FINAL_IDENTIFIER "${IDENTIFIER_PREFIX}${ASSET_IDENTIFIER}")
        file(
            APPEND "${ASSET_HEADER_PATH}"
            "RR_INCBIN(${ASSET_FINAL_IDENTIFIER}, \"${AssetAbsolutePath}\");\n"
        )
    endforeach (ASSET_PATH)
    foreach (ASSET_PATH ${ASSETS_LIST})
        cmake_path(ABSOLUTE_PATH ASSET_PATH NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

        get_filename_component(ASSET_NAME ${ASSET_PATH} NAME)
        string(MAKE_C_IDENTIFIER "${ASSET_NAME}" ASSET_IDENTIFIER)
        string(TOUPPER "${ASSET_IDENTIFIER}" ASSET_IDENTIFIER)
        set(ASSET_FINAL_IDENTIFIER "${IDENTIFIER_PREFIX}${ASSET_IDENTIFIER}")
        file(
            APPEND "${ASSET_HEADER_PATH}"
            "RR_INCBIN_REF(${ASSET_FINAL_IDENTIFIER});\n"
        )
    endforeach (ASSET_PATH)
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#else\n"
    )
    foreach (ASSET_PATH ${ASSETS_LIST})
        cmake_path(ABSOLUTE_PATH ASSET_PATH NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

        get_filename_component(ASSET_NAME ${ASSET_PATH} NAME)
        string(MAKE_C_IDENTIFIER "${ASSET_NAME}" ASSET_IDENTIFIER)
        string(TOUPPER "${ASSET_IDENTIFIER}" ASSET_IDENTIFIER)
        set(ASSET_FINAL_IDENTIFIER "${IDENTIFIER_PREFIX}${ASSET_IDENTIFIER}")
        file(
            APPEND "${ASSET_HEADER_PATH}"
            "RR_ASSET_EXTERN const Rr_AssetRef ${ASSET_FINAL_IDENTIFIER};\n"
        )
    endforeach (ASSET_PATH)
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#endif\n"
    )
endif()
