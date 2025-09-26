file(
    WRITE "${ASSET_HEADER_PATH}"
    ""
)
string(MAKE_C_IDENTIFIER "${ASSET_HEADER_NAME}_INC" ASSET_HEADER_DEFINE)
string(TOUPPER "${ASSET_HEADER_DEFINE}" ASSET_HEADER_DEFINE)
if(USE_RC)
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#ifdef RC_INVOKED\n"
    )
    foreach (ASSET_PATH ${ASSETS_LIST})
        cmake_path(ABSOLUTE_PATH ASSET_PATH NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

        get_filename_component(ASSET_NAME ${ASSET_PATH} NAME)
        string(MAKE_C_IDENTIFIER "${ASSET_NAME}" ASSET_IDENTIFIER)
        string(TOUPPER "${ASSET_IDENTIFIER}" ASSET_IDENTIFIER)
        set(ASSET_FINAL_IDENTIFIER "${IDENTIFIER_PREFIX}${ASSET_IDENTIFIER}")
        file(
            APPEND "${ASSET_HEADER_PATH}"
            "\"${ASSET_FINAL_IDENTIFIER}_ID\" RRDATA \"${AssetAbsolutePath}\"\n"
        )
    endforeach (ASSET_PATH)
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#else\n"
        "#ifndef ${ASSET_HEADER_DEFINE}\n"
        "#define ${ASSET_HEADER_DEFINE}\n"
        "#include <Rr/Rr_Asset.h>\n"
    )
    foreach (ASSET_PATH ${ASSETS_LIST})
        cmake_path(ABSOLUTE_PATH ASSET_PATH NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

        get_filename_component(ASSET_NAME ${ASSET_PATH} NAME)
        string(MAKE_C_IDENTIFIER "${ASSET_NAME}" ASSET_IDENTIFIER)
        string(TOUPPER "${ASSET_IDENTIFIER}" ASSET_IDENTIFIER)
        set(ASSET_FINAL_IDENTIFIER "${IDENTIFIER_PREFIX}${ASSET_IDENTIFIER}")
        file(
            APPEND "${ASSET_HEADER_PATH}"
            "static const Rr_AssetRef ${ASSET_FINAL_IDENTIFIER} = { \"${ASSET_FINAL_IDENTIFIER}_ID\" };\n"
        )
    endforeach (ASSET_PATH)
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#endif\n"
        "#endif\n"
        "\n"
    )
else()
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#include <Rr/Rr_Asset.h>\n"
    )
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#if defined(RR_INCBIN_ASSETS)\n"
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
        "#ifndef ${ASSET_HEADER_DEFINE}\n"
        "#define ${ASSET_HEADER_DEFINE}\n"
    )
    foreach (ASSET_PATH ${ASSETS_LIST})
        cmake_path(ABSOLUTE_PATH ASSET_PATH NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

        get_filename_component(ASSET_NAME ${ASSET_PATH} NAME)
        string(MAKE_C_IDENTIFIER "${ASSET_NAME}" ASSET_IDENTIFIER)
        string(TOUPPER "${ASSET_IDENTIFIER}" ASSET_IDENTIFIER)
        set(ASSET_FINAL_IDENTIFIER "${IDENTIFIER_PREFIX}${ASSET_IDENTIFIER}")
        file(
            APPEND "${ASSET_HEADER_PATH}"
            "RR_EXTERN const Rr_AssetRef ${ASSET_FINAL_IDENTIFIER};\n"
        )
    endforeach (ASSET_PATH)
    file(
        APPEND "${ASSET_HEADER_PATH}"
        "#endif\n"
        "#endif\n"
        "\n"
    )
endif()
