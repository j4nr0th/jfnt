//
// Created by jan on 19.8.2023.
//

#include "../include/jfnt_error.h"

static const struct {const char* message; const char* name;} RESULT_MESSAGES[JFNT_RESULT_COUNT] =
        {
                [JFNT_RESULT_SUCCESS] = {.message = "Success", .name = "JFNT_RESULT_SUCCESS"},
                [JFNT_RESULT_BAD_ALLOC] = {.message = "Memory allocation failed", .name = "JFNT_RESULT_BAD_ALLOC"},
                [JFNT_RESULT_NO_FC_MATCH] = {.message = "Fontconfig found no matching font", .name = "JFNT_RESULT_NO_FC_MATCH"},
                [JFNT_RESULT_BAD_FC_CALL] = {.message = "Fontconfig function failed", .name = "JFNT_RESULT_BAD_FC_CALL"},
                [JFNT_RESULT_BAD_FT_CALL] = {.message = "FreeType function failed", .name = "JFNT_RESULT_BAD_FT_CALL"},
                [JFNT_RESULT_UNSUPPORTED] = {.message = "Requested character was not supported by the font, nor could a suitable replacement be found", .name = "JFNT_RESULT_UNSUPPORTED"},
                [JFNT_RESULT_BAD_ENCODING] = {.message = "String was not encoded according to the expected format", .name = "JFNT_RESULT_BAD_ENCODING"},
                [JFNT_RESULT_NO_FC] = {.message = "Fontconfig could not be initialized", .name = "JFNT_RESULT_NO_FC"},
        };

const char* jfnt_result_to_str(jfnt_result res)
{
    if (res < JFNT_RESULT_SUCCESS || res >= JFNT_RESULT_COUNT)
        return "Invalid";
    return RESULT_MESSAGES[res].name;
}

const char* jfnt_result_message(jfnt_result res)
{
    if (res < JFNT_RESULT_SUCCESS || res >= JFNT_RESULT_COUNT)
        return "Invalid";
    return RESULT_MESSAGES[res].message;
}
