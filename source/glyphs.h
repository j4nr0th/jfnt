//
// Created by jan on 19.8.2023.
//

#ifndef JFNT_GLYPHS_H
#define JFNT_GLYPHS_H
#include "error.h"
#include <uchar.h>

struct jfnt_glyph_T
{
    char32_t codepoint;
    signed short top, left;
    unsigned short w, h;
    unsigned short advance_x, advance_y;
    unsigned int offset_x;
};
typedef struct jfnt_glyph_T jfnt_glyph;



#endif //JFNT_GLYPHS_H
