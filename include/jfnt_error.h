//
// Created by jan on 19.8.2023.
//

#ifndef JFNT_JFNT_ERROR_H
#define JFNT_JFNT_ERROR_H
enum jfnt_result_T
{
    JFNT_RESULT_SUCCESS = 0,

    JFNT_RESULT_BAD_ALLOC,

    JFNT_RESULT_NO_FC,
    JFNT_RESULT_BAD_FC_CALL,
    JFNT_RESULT_NO_FC_MATCH,

    JFNT_RESULT_BAD_FT_CALL,

    JFNT_RESULT_UNSUPPORTED,

    JFNT_RESULT_BAD_ENCODING,

    JFNT_RESULT_COUNT,
};
typedef enum jfnt_result_T jfnt_result;

const char* jfnt_result_to_str(jfnt_result res);

const char* jfnt_result_message(jfnt_result res);


#endif //JFNT_JFNT_ERROR_H
