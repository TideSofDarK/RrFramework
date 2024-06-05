find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(
        WRITE "${ResultFilePath}"
        "#ifndef RC_INVOKED\n"
        "#include <Rr/Rr_Asset.h>\n"
        "#endif\n"
        "\n"
        "#if defined(RR_USE_RC)\n"
        "\n"
)
foreach (AssetPath ${AssetsList})
    cmake_path(ABSOLUTE_PATH AssetPath NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

    get_filename_component(AssetName ${AssetPath} NAME)
    string(MAKE_C_IDENTIFIER "${AssetName}" AssetIdentifier)
    string(TOUPPER "${AssetIdentifier}" AssetIdentifier)
    set(FinalIdentifier "${IdentifierPrefix}${AssetIdentifier}")
    file(
            APPEND "${ResultFilePath}"
            "#define ${FinalIdentifier} \"${FinalIdentifier}_ID\"\n"
            "#if defined(RC_INVOKED)\n"
            "${FinalIdentifier}_ID RRDATA \"${AssetAbsolutePath}\"\n"
            "#endif\n"
            "\n"
    )
endforeach (AssetPath)
file(
        APPEND "${ResultFilePath}"
        "#else\n"
        "\n"
)
foreach (AssetPath ${AssetsList})
    cmake_path(ABSOLUTE_PATH AssetPath NORMALIZE OUTPUT_VARIABLE AssetAbsolutePath)

    get_filename_component(AssetName ${AssetPath} NAME)
    string(MAKE_C_IDENTIFIER "${AssetName}" AssetIdentifier)
    string(TOUPPER "${AssetIdentifier}" AssetIdentifier)
    set(FinalIdentifier "${IdentifierPrefix}${AssetIdentifier}")
    file(
            APPEND "${ResultFilePath}"
            "#if defined(RR_DEFINE_ASSETS)\n"
            "INCBIN(${FinalIdentifier}, \"${AssetAbsolutePath}\");\n"
            "#else\n"
            "RR_ASSET_EXTERN const Rr_AssetRef ${FinalIdentifier};\n"
            "#endif\n"
            "\n"
    )
endforeach (AssetPath)
file(
        APPEND "${ResultFilePath}"
        "#endif\n"
)
