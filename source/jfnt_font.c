//
// Created by jan on 19.8.2023.
//

#include <assert.h>
#include "../include/jfnt_font.h"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

struct jfnt_bitmap_T
{
    unsigned width;
    unsigned height;
    unsigned char* data;
};
typedef struct jfnt_bitmap_T jfnt_bitmap;
struct jfnt_font_T
{
    jfnt_allocator_callbacks allocator_callbacks;
    jfnt_error_callbacks error_callbacks;

    unsigned count_glyphs;
    unsigned capacity_glyphs;
    jfnt_glyph* glyphs;

    unsigned int size_x, size_y;
    unsigned int height;
    jfnt_bitmap bmp;
    unsigned tex_w;
    unsigned tex_h;
    unsigned average_width;
    int ascent; int descent;
};

static void* default_alloc_fn(void* state, size_t size)
{
    (void) state;
    assert(state == (void*)0xBadBeefCafe);
    return malloc(size);
}

static void* default_realloc_fn(void* state, void* ptr, size_t new_size)
{
    (void) state;
    assert(state == (void*)0xBadBeefCafe);
    return realloc(ptr, new_size);
}

static void default_free_fn(void* state, void* ptr)
{
    (void) state;
    assert(state == (void*)0xBadBeefCafe);
    free(ptr);
}

//  Proudly stolen from RXVT-Unicode rxvtfont.C
static const char32_t EXTENT_TEST_CHAR_ARRAY[] =
        {
                '0', '1', '8', 'a', 'd', 'x', 'm', 'y', 'g', 'W', 'X', '\'', '_',
                0x00cd, 0x00d5, 0x0114, 0x0177, 0x0643,	// some accented and arabic stuff
                0x304c, 0x672c,				// some chinese characters
                0xFFFD, //  � - replacement missing character
        };
static const unsigned EXTENT_CHAR_COLS[] =
        {
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 2,	                        // some accented and arabic stuff
                2, 2,				                    // some chinese characters
                2,                                      //  � - replacement missing character
        };

static const size_t EXTENT_TEST_CHAR_COUNT = sizeof(EXTENT_TEST_CHAR_ARRAY) / sizeof(*EXTENT_TEST_CHAR_ARRAY);

static FcPattern* font_match(FcPattern* pat, FcResult* res)
{
    FcConfigSubstitute(NULL, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);
    return FcFontMatch(NULL, pat, res);
}

static const char* const FC_ERRORS[] =
        {
                [FcResultMatch] = "FcResultMatch",
                [FcResultNoMatch] = "FcResultNoMatch",
                [FcResultTypeMismatch] = "FcResultTypeMismatch",
                [FcResultNoId] = "FcResultNoId",
                [FcResultOutOfMemory] = "FcResultOutOfMemory"
        };

static void load_font_data_from_face(jfnt_font* this, const FcMatrix* mtx, unsigned font_x_size, unsigned font_y_size, FT_Face face)
{
    unsigned height;
    FT_Set_Char_Size(face, font_x_size, font_y_size, 0, 0);
    if (mtx->xx != 1.0 || mtx->xy != 0 || mtx->yx != 0 || mtx->yy != 1)
    {
        FT_Matrix ft_mat = {
                .xx = (FT_Fixed) (0x10000L * mtx->xx),
                .xy = (FT_Fixed) (0x10000L * mtx->xy),
                .yx = (FT_Fixed) (0x10000L * mtx->yx),
                .yy = (FT_Fixed) (0x10000L * mtx->yy),
        };

        FT_Set_Transform(face, &ft_mat, NULL);


        FT_Vector v;

        v.x = 0;
        v.y = face->size->metrics.height;
        FT_Vector_Transform(&v, &ft_mat);
        height = (unsigned) (v.y >> 6);
        this->ascent = (int)(face->size->metrics.ascender >> 6);
        this->descent = (int)(face->size->metrics.descender >> 6);
    }
    else
    {
        height = (unsigned)(face->size->metrics.height >> 6);
        this->ascent = (int)(face->size->metrics.ascender >> 6);
        this->descent = (int)(face->size->metrics.descender >> 6);
    }

    this->height = height;
    this->size_x = font_x_size;
    this->size_y = font_y_size;

    //  Compute jfnt_font sizes
    for (unsigned i = 0; i < EXTENT_TEST_CHAR_COUNT; ++i)
    {
        const FcChar32 cp = EXTENT_TEST_CHAR_ARRAY[i];
        //  Check if jfnt_font supports the codepoint
//        if ((ft_res = FT_Load_Char(face, cp, FT_LOAD_DEFAULT)) != FT_Err_Ok)
        if (FT_Load_Char(face, cp, FT_LOAD_DEFAULT) != FT_Err_Ok)
        {
            continue;
        }
        FT_GlyphSlot glyph = face->glyph;
        const unsigned char_cols = EXTENT_CHAR_COLS[i];
        unsigned advance = (glyph->advance.x >> 6);
        if (advance < glyph->bitmap.width)
        {
            advance = glyph->bitmap.width;
        }
        if (char_cols > 1)
        {
            advance = (advance + char_cols - 1) / char_cols;
        }
        if (advance > this->average_width)
        {
            this->average_width = advance;
        }
    }

}

static jfnt_result
font_create_from_pattern(
        jfnt_font* this, FcPattern* pattern, const char* prefix, const char* name, FT_Library ft_library,
        FT_Face* p_face)
{
    FcResult fc_result;
    assert(pattern);

    FcChar8* filename;
    switch ((fc_result = FcPatternGetString(pattern, FC_FILE, 0, &filename)))
    {
    case FcResultNoMatch:
    JFNT_ERROR(this, "Found no file for jfnt_font %s \"%s\": %u\n", prefix, name, fc_result);
        return JFNT_RESULT_NO_FC_MATCH;
    case FcResultMatch:break;
    default:
    JFNT_ERROR(this, "Function FcPatternGetString returned unknown status: %u\n", fc_result);
        return JFNT_RESULT_BAD_FC_CALL;
    }

    int font_id;
    switch ((fc_result = FcPatternGetInteger(pattern, FC_INDEX, 0, &font_id)))
    {
    case FcResultNoMatch:
        filename = NULL;
        break;
    case FcResultMatch:break;
    default:
    JFNT_ERROR(this, "Function FcPatternGetString returned unknown status: %u\n", fc_result);
        return JFNT_RESULT_BAD_FC_CALL;
    }

    double pixel_size;
    if ((fc_result = FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &pixel_size)) != FcResultMatch)
    {
        JFNT_ERROR(this, "Could not find jfnt_font pixel size for jfnt_font %s \"%s\", reason: %s\n", prefix, name, FC_ERRORS[fc_result]);
        return JFNT_RESULT_BAD_FC_CALL;
    }
    double aspect_ratio;
    if ((fc_result = FcPatternGetDouble(pattern, FC_ASPECT, 0, &aspect_ratio)) != FcResultMatch)
    {
        aspect_ratio = 1;
    }
    FcMatrix m;
    FcMatrixInit(&m);
    FcMatrix* mtx;
    if ((fc_result = FcPatternGetMatrix(pattern, FC_MATRIX, 0, &mtx)) != FcResultMatch)
    {
        mtx = &m;
        if (!mtx)
        {
            JFNT_ERROR(this, "Could not allocate matrix for jfnt_font %s \"%s\", reason: %s\n", prefix, name, FC_ERRORS[fc_result]);
            return JFNT_RESULT_BAD_ALLOC;
        }
    }

    const unsigned font_y_size = (unsigned)pixel_size << 6;
    const unsigned font_x_size = (unsigned)(pixel_size * aspect_ratio) << 6;
    int char_width = -1;


    switch ((fc_result = FcPatternGetInteger(pattern, FC_CHAR_WIDTH, 0, &char_width)))
    {
    case FcResultNoMatch:
        char_width = 0;
        break;
    case FcResultMatch:break;
    default:
    JFNT_ERROR(this, "Function FcPatternGetInteger returned unknown status: %u\n", fc_result);
        return JFNT_RESULT_BAD_FC_CALL;
    }


    FcCharSet* cs;
    if ((fc_result = FcPatternGetCharSet(pattern, FC_CHARSET, 0, &cs)) != FcResultMatch)
    {
        JFNT_ERROR(this, "Could not find jfnt_font charset for jfnt_font %s \"%s\", reason: %s\n", prefix, name, FC_ERRORS[fc_result]);
        return JFNT_RESULT_BAD_FC_CALL;
    }
    FcCharSet* char_set = FcCharSetCopy(cs);
    cs = NULL;
    if (!char_set)
    {
        JFNT_ERROR(this, "Could not duplicate jfnt_font charset for jfnt_font %s \"%s\"\n", prefix, name);
        return JFNT_RESULT_BAD_FC_CALL;
    }

    this->average_width = char_width;

    FT_Face  face;
    FT_Error ft_res = FT_New_Face(ft_library, (const char*)filename, font_id, &face);
    if (ft_res != FT_Err_Ok)
    {
        JFNT_ERROR(this, "Could not create new face for jfnt_font %s \"%s\" from file \"%s\", reason: %s\n", prefix, name, filename,
                   FT_Error_String(ft_res));
        return JFNT_RESULT_BAD_FT_CALL;
    }

    load_font_data_from_face(this, mtx, font_x_size, font_y_size, face);
    this->average_width = char_width;

    FcPatternDestroy(pattern);
    *p_face = face;

    return JFNT_RESULT_SUCCESS;
}


static void font_add_to_bitmap(const jfnt_bitmap* dst, const jfnt_bitmap* source, unsigned offset_x)
{
    for (unsigned row = 0; row < source->height; ++row)
    {
        memcpy(dst->data + (row) * dst->width + offset_x, source->data + (row) * source->width, source->width);
    }
}

static jfnt_result
ft_font_load(FT_Face font, unsigned range_count, const jfnt_codepoint_range* range_array, jfnt_font* fnt, int flip)
{
    unsigned n_chars = 0;
    unsigned max_w = 0, max_h = 0, total_w = 0;
    for (unsigned i_range = 0; i_range < range_count; ++i_range)
    {
        const jfnt_codepoint_range range = range_array[i_range];
        n_chars += 1 + range.last - range.first;

        FT_GlyphSlot glyph = font->glyph;
        FT_Error ft_res;
        for (FT_ULong c = range.first; c <= range.last; ++c)
        {
            if ((ft_res = FT_Load_Char(font, c, FT_LOAD_BITMAP_METRICS_ONLY)) != FT_Err_Ok)
            {
                if (fnt->error_callbacks.unsupported_char)
                {
                    fnt->error_callbacks.unsupported_char(fnt, c, FT_Error_String(ft_res), fnt->error_callbacks.char_param);
                }
                continue;
            }
            const unsigned bmp_w = glyph->bitmap.width;
            const unsigned bmp_h = glyph->bitmap.rows;

            if (max_w < bmp_w) max_w = bmp_w;
            if (max_h < bmp_h) max_h = bmp_h;
            total_w += (bmp_w + 1);
        }
        if (total_w) total_w -= 1;
    }

    unsigned char* const tmp = flip ? jfnt_alloc(fnt, max_w) : NULL;
    if (flip && !tmp)
    {
        return JFNT_RESULT_BAD_ALLOC;
    }
    jfnt_bitmap bmp = {.width = total_w, .height = max_h, .data = jfnt_alloc(fnt, total_w * max_h)};
    if (!bmp.data)
    {
        jfnt_free(fnt, tmp);
        return JFNT_RESULT_BAD_ALLOC;
    }
    memset(bmp.data, 0, sizeof(total_w * max_h));

    jfnt_glyph* const glyphs = calloc(n_chars, sizeof(*glyphs));
    if (!glyphs)
    {
        jfnt_free(fnt, tmp);
        jfnt_free(fnt, bmp.data);
        return JFNT_RESULT_BAD_ALLOC;
    }

    unsigned i_char = 0;
    unsigned current_x = 0;
    for (unsigned i_range = 0; i_range < range_count; ++i_range)
    {
        const jfnt_codepoint_range range = range_array[i_range];

        FT_GlyphSlot glyph = font->glyph;
        for (FT_ULong c = range.first; c <= range.last; ++c)
        {
            if (FT_Load_Char(font, c, FT_LOAD_RENDER) != FT_Err_Ok)
            {
                continue;
            }
            const unsigned advance_x = glyph->advance.x >> 6;
            const unsigned advance_y = glyph->advance.y >> 6;

            const unsigned bmp_w = glyph->bitmap.width;
            const unsigned bmp_h = glyph->bitmap.rows;

            glyphs[i_char].advance_x = advance_x;
            glyphs[i_char].advance_y = advance_y;
            glyphs[i_char].w = bmp_w;
            glyphs[i_char].h = bmp_h;
            glyphs[i_char].left = (short)glyph->bitmap_left;
            glyphs[i_char].top = (short)glyph->bitmap_top;
            glyphs[i_char].offset_x = current_x;
            glyphs[i_char].codepoint = (char32_t)c;


            if (flip)
            {
                for (unsigned r = 0; r < bmp_h / 2; ++r)
                {
                    memcpy(tmp, glyph->bitmap.buffer + (bmp_w * r), bmp_w);
                    memcpy(glyph->bitmap.buffer + (bmp_w * r), glyph->bitmap.buffer + (bmp_w * (bmp_h - r - 1)), bmp_w);
                    memcpy(glyph->bitmap.buffer + (bmp_w * (bmp_h - r - 1)), tmp, bmp_w);
                }
            }
            const jfnt_bitmap t = {.data = glyph->bitmap.buffer, .width = bmp_w, .height = bmp_h};
            font_add_to_bitmap(&bmp, &t, current_x);

            current_x += (bmp_w + 1);
            i_char += 1;
        }
    }
    jfnt_free(fnt, tmp);

    fnt->bmp = bmp;
    fnt->glyphs = glyphs;
    fnt->capacity_glyphs = n_chars;
    fnt->count_glyphs = n_chars;
    return JFNT_RESULT_SUCCESS;
}

static jfnt_result font_create_from_fc_name(jfnt_font* this, const char* name, FT_Library ft_lib, FT_Face* p_face)
{
    const FcBool loaded_fc = FcInit();
    if (!loaded_fc)
    {
        JFNT_ERROR(this, "Could not initialize fontconfig");
        return JFNT_RESULT_NO_FC;
    }

    FcPattern* pattern = FcNameParse((const unsigned char*)name);
    if (!pattern)
    {
        JFNT_ERROR(this, "Could not create jfnt_font from name \"%s\", reason: invalid name\n", name);
        return JFNT_RESULT_NO_FC_MATCH;
    }
    FcResult fc_result;
    FcPattern* const match = font_match(pattern, &fc_result);

    const jfnt_result res = font_create_from_pattern(this, match, "from name", name, ft_lib, p_face);

    FcPatternDestroy(pattern);

    return res;
}


const jfnt_allocator_callbacks DEFAULT_ALLOCATOR =
        {
        .state = (void*)0xBadBeefCafe,
        .allocate = default_alloc_fn,
        .reallocate = default_realloc_fn,
        .deallocate = default_free_fn,
        };

jfnt_result
jfnt_font_create_from_memory(
        size_t size, const void* mem, unsigned char_size, jfnt_font_create_info create_info, jfnt_font** p_out)
{
    jfnt_font_create_info info = create_info;
    if (!info.allocator_callbacks)
    {
        info.allocator_callbacks = &DEFAULT_ALLOCATOR;
    }
    jfnt_font* const this = info.allocator_callbacks->allocate(info.allocator_callbacks->state, sizeof(*this));
    if (!this)
    {
        return JFNT_RESULT_BAD_ALLOC;
    }
#ifndef NDEBUG
    memset(this, 0xAB, sizeof(*this));
#endif
    this->allocator_callbacks = *info.allocator_callbacks;
    this->error_callbacks = *info.error_callbacks;

    FT_Library ft_library;
    FT_Error ft_error = FT_Init_FreeType(&ft_library);
    if (ft_error != FT_Err_Ok)
    {
        JFNT_ERROR(this, "Could not init FreeType library, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        return JFNT_RESULT_BAD_FT_CALL;
    }

    FT_Face face;
    ft_error = FT_New_Memory_Face(ft_library, mem, (FT_Long)size, 0, &face);
    if (ft_error != FT_Err_Ok)
    {
        JFNT_ERROR(this, "Could not create new FT memory face, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        FT_Done_FreeType(ft_library);
        return JFNT_RESULT_BAD_FT_CALL;
    }

    ft_error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if (ft_error != FT_Err_Ok)
    {
        JFNT_ERROR(this, "Could not select FT Face encoding, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        FT_Done_Face(face);
        FT_Done_FreeType(ft_library);
        return JFNT_RESULT_BAD_FT_CALL;
    }

//    ft_error = FT_Set_Pixel_Sizes(face, 0, char_size);
//    if (ft_error != FT_Err_Ok)
//    {
//        JFNT_ERROR(this, "Could not set FT Face size, reason: %s", FT_Error_String(ft_error));
//        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
//        FT_Done_Face(face);
//        FT_Done_FreeType(ft_library);
//        return JFNT_RESULT_BAD_FT_CALL;
//    }

    FcMatrix mtx;
    FcMatrixInit(&mtx);
    load_font_data_from_face(this, &mtx, char_size, char_size, face);

    jfnt_result res = ft_font_load(face, info.n_ranges, info.codepoint_ranges, this, info.flip);

    FT_Done_Face(face);
    FT_Done_FreeType(ft_library);
    if (res != JFNT_RESULT_SUCCESS)
    {
        JFNT_ERROR(this, "Could not select FT Face encoding, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        return res;
    }

    *p_out = this;
    return JFNT_RESULT_SUCCESS;
}

void* jfnt_alloc(const jfnt_font* font, size_t size)
{
    return font->allocator_callbacks.allocate(font->allocator_callbacks.state, size);
}

void* jfnt_realloc(const jfnt_font* font, void* ptr, size_t new_size)
{
    return font->allocator_callbacks.reallocate(font->allocator_callbacks.state, ptr, new_size);;
}

void jfnt_free(const jfnt_font* font, void* ptr)
{
    font->allocator_callbacks.deallocate(font->allocator_callbacks.state, ptr);
}

void jfnt_report_error(const jfnt_font* font, const char* file, int line, const char* function, const char* fmt, ...)
{
    if (!font->error_callbacks.report)
    {
        return;
    }
    va_list args, cpy;
    va_start(args, fmt);
    va_copy(cpy, args);
    const int len = vsnprintf(NULL, 0, fmt, cpy);
    va_end(cpy);
    if (len <= 0)
    {
        va_end(args);
        return;
    }
    char* const buffer = jfnt_alloc(font, len + 1);
    if (!buffer)
    {
        return;
    }
    (void) vsnprintf(buffer, len + 1, fmt, args);
    va_end(args);
    font->error_callbacks.report(buffer, function, file, line, font->error_callbacks.report_param);
    jfnt_free(font, buffer);
}

jfnt_result jfnt_font_create_from_filename(
        const char* filename, unsigned char_size, jfnt_font_create_info create_info, jfnt_font** p_out)
{
    jfnt_font_create_info info = create_info;
    if (!info.allocator_callbacks)
    {
        info.allocator_callbacks = &DEFAULT_ALLOCATOR;
    }
    jfnt_font* const this = info.allocator_callbacks->allocate(info.allocator_callbacks->state, sizeof(*this));
    if (!this)
    {
        return JFNT_RESULT_BAD_ALLOC;
    }
#ifndef NDEBUG
    memset(this, 0xAB, sizeof(*this));
#endif
    this->allocator_callbacks = *info.allocator_callbacks;
    this->error_callbacks = *info.error_callbacks;

    FT_Library ft_library;
    FT_Error ft_error = FT_Init_FreeType(&ft_library);
    if (ft_error != FT_Err_Ok)
    {
        JFNT_ERROR(this, "Could not init FreeType library, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        return JFNT_RESULT_BAD_FT_CALL;
    }

    FT_Face face;
    ft_error = FT_New_Face(ft_library, filename, 0, &face);
    if (ft_error != FT_Err_Ok)
    {
        JFNT_ERROR(this, "Could not create new FT memory face, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        FT_Done_FreeType(ft_library);
        return JFNT_RESULT_BAD_FT_CALL;
    }

    ft_error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if (ft_error != FT_Err_Ok)
    {
        JFNT_ERROR(this, "Could not select FT Face encoding, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        FT_Done_Face(face);
        FT_Done_FreeType(ft_library);
        return JFNT_RESULT_BAD_FT_CALL;
    }

    FcMatrix mtx;
    FcMatrixInit(&mtx);
    load_font_data_from_face(this, &mtx, char_size, char_size, face);

    jfnt_result res = ft_font_load(face, info.n_ranges, info.codepoint_ranges, this, info.flip);
    FT_Done_Face(face);
    FT_Done_FreeType(ft_library);
    if (res != JFNT_RESULT_SUCCESS)
    {
        JFNT_ERROR(this, "Could not select FT Face encoding, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        return res;
    }

    *p_out = this;
    return JFNT_RESULT_SUCCESS;
}

jfnt_result jfnt_font_create_from_fc_str(const char* fc_str, jfnt_font_create_info create_info, jfnt_font** p_out)
{
    jfnt_font_create_info info = create_info;
    if (!info.allocator_callbacks)
    {
        info.allocator_callbacks = &DEFAULT_ALLOCATOR;
    }
    jfnt_font* const this = info.allocator_callbacks->allocate(info.allocator_callbacks->state, sizeof(*this));
    if (!this)
    {
        return JFNT_RESULT_BAD_ALLOC;
    }
#ifndef NDEBUG
    memset(this, 0xAB, sizeof(*this));
#endif
    this->allocator_callbacks = *info.allocator_callbacks;
    this->error_callbacks = *info.error_callbacks;

    FT_Library ft_library;
    FT_Error ft_error = FT_Init_FreeType(&ft_library);
    if (ft_error != FT_Err_Ok)
    {
        JFNT_ERROR(this, "Could not init FreeType library, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        return JFNT_RESULT_BAD_FT_CALL;
    }

    FT_Face face;
    jfnt_result res = font_create_from_fc_name(this, fc_str, ft_library, &face);
    if (res != JFNT_RESULT_SUCCESS)
    {
        JFNT_ERROR(this, "Could not create font from FC string, reason: %s (%s)", jfnt_result_to_str(res), jfnt_result_message(res));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        FT_Done_FreeType(ft_library);
        return res;
    }
    res = ft_font_load(face, info.n_ranges, info.codepoint_ranges, this, info.flip);
    
    FT_Done_Face(face);
    FT_Done_FreeType(ft_library);
    if (res != JFNT_RESULT_SUCCESS)
    {
        JFNT_ERROR(this, "Could not select FT Face encoding, reason: %s", FT_Error_String(ft_error));
        info.allocator_callbacks->deallocate(info.allocator_callbacks->state, this);
        return res;
    }


    *p_out = this;
    return JFNT_RESULT_SUCCESS;
}

void jfnt_font_destroy(jfnt_font* font)
{
    jfnt_free(font, font->bmp.data);
    jfnt_free(font, font->glyphs);
    jfnt_free(font, font);
}

static int find_glyph(const jfnt_font* font, char32_t c)
{
    //  Use binary search to narrow down the search to 8
    unsigned len = font->count_glyphs;
    unsigned pos = 0;
    while (len > 8)
    {
        unsigned step = (len) / 2;
        if (font->glyphs[pos + step].codepoint > c)
        {
            len = step;
        }
        else //  font->glyphs[pos + step].codepoint <= c
        {
            pos += step;
            len -= step;
        }
    }

    while (font->glyphs[pos].codepoint < c)
    {
        pos += 1;
    }

    return font->glyphs[pos].codepoint == c ? (int)pos : -1;
}

jfnt_result jfnt_font_find_glyphs_u32(
        const jfnt_font* font, char32_t unsupported_replace, size_t count, const char32_t* codepoints, int* p_indices)
{
    int i_replace = -1;
    for (size_t i = 0; i < count; ++i)
    {
        int idx = find_glyph(font, codepoints[i]);
        if (idx == -1)
        {
            if (i_replace == -1)
            {
                i_replace = find_glyph(font, unsupported_replace);
                if (i_replace == -1)
                {
                    JFNT_ERROR(font, "The unsupported replacement character U+%04hX (%lc) was not supported by the font", unsupported_replace, (wchar_t)unsupported_replace);
                    return JFNT_RESULT_UNSUPPORTED;
                }
            }
            idx = i_replace;
        }
        p_indices[i] = idx;
    }

    return JFNT_RESULT_SUCCESS;
}

const jfnt_glyph* jfnt_font_get_glyphs(const jfnt_font* font)
{
    return font->glyphs;
}

jfnt_result jfnt_font_find_glyphs_utf8(
        const jfnt_font* font, const char* utf8, char32_t unsupported_replace, size_t max_len, size_t* p_count,
        int* p_indices)
{
    int i_replace = -1;
    size_t i = 0;
    for (const unsigned char* ptr = (const unsigned char*)utf8; *ptr && i < max_len; ++ptr)
    {
        const unsigned char zb = *ptr;
        char32_t c = zb;
        if (c < 0x80)
        {
            //  One byte character
            ;   //  No-op
        }
        else if ((zb & 0xE0) == 0xC0)
        {
            //  2 byte codepoint
            ptr += 1;
            unsigned char b = *ptr;
            if ((b & 0xC0) != 0x80)
            {
                JFNT_ERROR(font,
                           "Continuation byte expected to follow in %u index following %02hhX, instead %02hhX was found",
                           (unsigned) (ptr - (const unsigned char*) utf8), zb, b);
                return JFNT_RESULT_BAD_ENCODING;
            }
            c <<= 6;
            c |= (0x3F & b);
        }
        else if ((zb & 0xF0) == 0xE0)
        {
            //  Three byte codepoint
            ptr += 1;
            unsigned char b1 = *ptr;
            if ((b1 & 0xC0) != 0x80)
            {
                JFNT_ERROR(font,
                           "Continuation byte expected to follow in %u index following %02hhX, instead %02hhX was found",
                           (unsigned) (ptr - (const unsigned char*) utf8), zb, b1);
                return JFNT_RESULT_BAD_ENCODING;
            }
            c <<= 6;
            c |= (0x3F & b1);

            ptr += 1;
            unsigned char b2 = *ptr;
            if ((b2 & 0xC0) != 0x80)
            {
                JFNT_ERROR(font,
                           "Continuation byte expected to follow in %u index following %02hhX %02hhx, instead %02hhX was found",
                           (unsigned) (ptr - (const unsigned char*) utf8), zb, b1, b2);
                return JFNT_RESULT_BAD_ENCODING;
            }
            c <<= 6;
            c |= (0x3F & b2);
        }
        else if ((zb & 0xF8) == 0xF0)
        {
            //  Four byte codepoint
            ptr += 1;
            unsigned char b1 = *ptr;
            if ((b1 & 0xC0) != 0x80)
            {
                JFNT_ERROR(font,
                           "Continuation byte expected to follow in %u index following %02hhX, instead %02hhX was found",
                           (unsigned) (ptr - (const unsigned char*) utf8), zb, b1);
                return JFNT_RESULT_BAD_ENCODING;
            }
            c <<= 6;
            c |= (0x3F & b1);

            ptr += 1;
            unsigned char b2 = *ptr;
            if ((b2 & 0xC0) != 0x80)
            {
                JFNT_ERROR(font,
                           "Continuation byte expected to follow in %u index following %02hhX %02hhx, instead %02hhX was found",
                           (unsigned) (ptr - (const unsigned char*) utf8), zb, b1, b2);
                return JFNT_RESULT_BAD_ENCODING;
            }
            c <<= 6;
            c |= (0x3F & b2);

            ptr += 1;
            unsigned char b3 = *ptr;
            if ((b3 & 0xC0) != 0x80)
            {
                JFNT_ERROR(font,
                           "Continuation byte expected to follow in %u index following %02hhX %02hhx %02hhx, instead %02hhX was found",
                           (unsigned) (ptr - (const unsigned char*) utf8), zb, b1, b2, b3);
                return JFNT_RESULT_BAD_ENCODING;
            }
            c <<= 6;
            c |= (0x3F & b3);
        }
        else
        {
            JFNT_ERROR(font, "Byte %02hhx is invalid in UTF-8 encoding", zb);
            return JFNT_RESULT_BAD_ENCODING;
        }


        int idx = find_glyph(font, c);
        if (idx == -1)
        {
            if (i_replace == -1)
            {
                i_replace = find_glyph(font, unsupported_replace);
                if (i_replace == -1)
                {
                    JFNT_ERROR(font, "The unsupported replacement character U+%04hX (%lc) was not supported by the font", unsupported_replace, (wchar_t)unsupported_replace);
                    return JFNT_RESULT_UNSUPPORTED;
                }
            }
            idx = i_replace;
        }
        p_indices[i] = idx;
        i += 1;
    }
    *p_count = i;
    return JFNT_RESULT_SUCCESS;
}

void jfnt_font_image(const jfnt_font* font, unsigned int* p_width, unsigned int* p_height, const unsigned char** p_data)
{
    *p_width = font->bmp.width;
    *p_height = font->bmp.height;
    *p_data = font->bmp.data;
}

void
jfnt_font_get_sizes(const jfnt_font* font, unsigned* p_height, unsigned* p_avg_w, unsigned* p_size_h, unsigned* p_size_v)
{
    if (p_height)
    {
        *p_height = font->height;
    }
    if (p_avg_w)
    {
        *p_avg_w = font->average_width;
    }
    if (p_size_h)
    {
        *p_size_h = font->size_x;
    }
    if (p_size_v)
    {
        *p_size_v = font->size_y;
    }
}

void
jfnt_font_get_measures(const jfnt_font* font, unsigned* p_height, int* ascent, int* descent)
{
    if (p_height)
    {
        *p_height = font->height;
    }
    if (ascent)
    {
        *ascent = font->ascent;
    }
    if (descent)
    {
        *descent = font->descent;
    }
}
