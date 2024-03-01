#include "RrAsset.h"

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_log.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_JPEG
#define STBI_NO_GIF
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_NO_TGA
#define STBI_NO_FAILURE_STRINGS
// #define STBI_MALLOC Memory::Malloc
// #define STBI_REALLOC Memory::Realloc
// #define STBI_FREE Memory::Free

#include <stb/stb_image.h>

#include "RrArray.h"
#include <cglm/vec2.h>
#include <cglm/vec3.h>
#include <cglm/vec4.h>
#include <cglm/ivec3.h>

#include "RrTypes.h"

static size_t GetNewLine(const char* Data, size_t Length, size_t CurrentIndex)
{
    CurrentIndex++;
    while (CurrentIndex < Length && Data[CurrentIndex] != '\n')
    {
        CurrentIndex++;
    }

    return CurrentIndex;
}

SRrRawMesh RrRawMesh_FromOBJAsset(SRrAsset* Asset)
{
    /* Init scratch buffers. */
    static SRrArray ScratchPositions = { 0 };
    static SRrArray ScratchColors = { 0 };
    static SRrArray ScratchTexCoords = { 0 };
    static SRrArray ScratchNormals = { 0 };
    static SRrArray ScratchIndices = { 0 };

    RrArray_Init(&ScratchPositions, vec3, 1000);
    RrArray_Init(&ScratchColors, vec4, 1000);
    RrArray_Init(&ScratchTexCoords, vec2, 1000);
    RrArray_Init(&ScratchNormals, vec3, 1000);
    RrArray_Init(&ScratchIndices, ivec3, 1000);

    RrArray_Empty(ScratchPositions, false);
    RrArray_Empty(ScratchColors, false);
    RrArray_Empty(ScratchTexCoords, false);
    RrArray_Empty(ScratchNormals, false);
    RrArray_Empty(ScratchIndices, false);

    /* Parse OBJ data. */
    SRrRawMesh RawMesh = { 0 };

    RrArray_Init(&RawMesh.Vertices, SRrVertex, 1);
    RrArray_Init(&RawMesh.Indices, u32, 1);

    size_t CurrentIndex = 0;
    char* EndPos;
    while (CurrentIndex < Asset->Length)
    {
        switch (Asset->Data[CurrentIndex])
        {
            case 'v':
            {
                CurrentIndex++;
                switch (Asset->Data[CurrentIndex])
                {
                    case ' ':
                    {
                        vec3 NewPosition;
                        NewPosition[0] = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewPosition[1] = (float)SDL_strtod(EndPos, &EndPos);
                        NewPosition[2] = (float)SDL_strtod(EndPos, &EndPos);

                        RrArray_Push(&ScratchPositions, &NewPosition);

                        if (*EndPos == ' ')
                        {
                            vec4 NewColor = { 0 };
                            NewColor[0] = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor[1] = (float)SDL_strtod(EndPos, &EndPos);
                            NewColor[2] = (float)SDL_strtod(EndPos, &EndPos);

                            RrArray_Push(&ScratchColors, &NewColor);
                        }
                    }
                    break;
                    case 't':
                    {
                        CurrentIndex++;
                        vec2 NewTexCoord;
                        NewTexCoord[0] = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewTexCoord[1] = (float)SDL_strtod(EndPos, &EndPos);
                        RrArray_Push(&ScratchTexCoords, &NewTexCoord);
                    }
                    break;
                    case 'n':
                    {
                        CurrentIndex++;
                        vec3 NewNormal;
                        NewNormal[0] = (float)SDL_strtod(Asset->Data + CurrentIndex, &EndPos);
                        NewNormal[1] = (float)SDL_strtod(EndPos, &EndPos);
                        NewNormal[2] = (float)SDL_strtod(EndPos, &EndPos);
                        RrArray_Push(&ScratchNormals, &NewNormal);
                    }
                    break;
                }
            }
            break;
            case 'f':
            {
                CurrentIndex++;
                ivec3 OBJIndices[3] = { 0 };
                OBJIndices[0][0] = (i32)SDL_strtoul(Asset->Data + CurrentIndex, &EndPos, 10);
                OBJIndices[0][1] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[0][2] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1][0] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1][1] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[1][2] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2][0] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2][1] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                OBJIndices[2][2] = (i32)SDL_strtoul(EndPos + 1, &EndPos, 0);
                for (size_t Index = 0; Index < 3; Index++)
                {
                    glm_ivec3_subs(OBJIndices[Index], 1, OBJIndices[Index]);

                    size_t ExistingOBJIndex = SIZE_MAX;
                    for (size_t I = 0; I < RrArray_Count(ScratchIndices); I++)
                    {
                        if (glm_ivec3_eqv(OBJIndices[Index], ((ivec3*)ScratchIndices)[I]))
                        {
                            ExistingOBJIndex = I;
                            break;
                        }
                    }
                    if (ExistingOBJIndex == SIZE_MAX)
                    {
                        vec3* Position = RrArray_Get(ScratchPositions, OBJIndices[Index][0]);
                        vec4* Color = RrArray_Get(ScratchColors, OBJIndices[Index][0]);
                        vec2* TexCoord = RrArray_Get(ScratchTexCoords, OBJIndices[Index][1]);
                        vec3* Normal = RrArray_Get(ScratchNormals, OBJIndices[Index][2]);
                        SRrVertex NewVertex = { 0 };
                        glm_vec3_copy(Position[0], NewVertex.Position);
                        glm_vec4_copy(Color[0], NewVertex.Color);
                        glm_vec3_copy(Normal[0], NewVertex.Normal);
                        NewVertex.TexCoordX = *TexCoord[0];
                        NewVertex.TexCoordY = *TexCoord[1];
                        RrArray_Push(&RawMesh.Vertices, &NewVertex);

                        RrArray_Push(&ScratchIndices, OBJIndices[Index]);

                        /** Add freshly added vertex index */
                        RrArray_Push(&RawMesh.Indices, &(u32){ RrArray_Count( RawMesh.Vertices) - 1 });
                    }
                    else
                    {
                        RrArray_Push(&RawMesh.Indices, &ExistingOBJIndex);
                    }
                }
            }
            break;
            default:
            {
            }
            break;
        }
        CurrentIndex = GetNewLine(Asset->Data, Asset->Length, CurrentIndex) + 1;
    }
    return RawMesh;
}
