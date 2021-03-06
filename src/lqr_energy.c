/* LiquidRescaling Library
 * Copyright (C) 2007-2009 Carlo Baldassi (the "Author") <carlobaldassi@gmail.com>.
 * All Rights Reserved.
 *
 * This library implements the algorithm described in the paper
 * "Seam Carving for Content-Aware Image Resizing"
 * by Shai Avidan and Ariel Shamir
 * which can be found at http://www.faculty.idc.ac.il/arik/imret.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3 dated June, 2007.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <float.h> /* FLT_MAX */
#include "lqr_base.h"
#include "lqr_gradient.h"
#include "lqr_rwindow.h"
#include "lqr_energy.h"
#include "lqr_progress_pub.h"
#include "lqr_cursor_pub.h"
#include "lqr_vmap.h"
#include "lqr_vmap_list.h"
#include "lqr_carver_list.h"
#include "lqr_carver.h"

#ifdef __LQR_DEBUG__
#include <stdio.h>
#include <assert.h>
#endif /* __LQR_DEBUG__ */

/* read normalised pixel value from
 * rgb buffer at the given index */
double
lqr_pixel_get_norm(void *rgb, int rgb_ind, LqrColDepth col_depth)
{
    switch (col_depth) {
        case LQR_COLDEPTH_8I:
            return (double) AS_8I(rgb)[rgb_ind] / 0xFF;
        case LQR_COLDEPTH_16I:
            return (double) AS_16I(rgb)[rgb_ind] / 0xFFFF;
        case LQR_COLDEPTH_32F:
            return (double) AS_32F(rgb)[rgb_ind];
        case LQR_COLDEPTH_64F:
            return (double) AS_64F(rgb)[rgb_ind];
        default:
#ifdef __LQR_DEBUG__
            assert(0);
#endif /* __LQR_DEBUG__ */
            return 0;
    }
}

/* write pixel from normalised value
 * in rgb buffer at the given index */
void
lqr_pixel_set_norm(double val, void *rgb, int rgb_ind, LqrColDepth col_depth)
{
    switch (col_depth) {
        case LQR_COLDEPTH_8I:
            AS_8I(rgb)[rgb_ind] = AS0_8I(val * 0xFF);
            return;
        case LQR_COLDEPTH_16I:
            AS_16I(rgb)[rgb_ind] = AS0_16I(val * 0xFFFF);
            return;
        case LQR_COLDEPTH_32F:
            AS_32F(rgb)[rgb_ind] = AS0_32F(val);
            return;
        case LQR_COLDEPTH_64F:
            AS_64F(rgb)[rgb_ind] = AS0_64F(val);
            return;
        default:
#ifdef __LQR_DEBUG__
            assert(0);
#endif /* __LQR_DEBUG__ */
            return;
    }
}

double
lqr_pixel_get_rgbcol(void *rgb, int rgb_ind, LqrColDepth col_depth, LqrImageType image_type, int channel)
{
    double black_fact = 0;

    switch (image_type) {
        case LQR_RGB_IMAGE:
        case LQR_RGBA_IMAGE:
            return lqr_pixel_get_norm(rgb, rgb_ind + channel, col_depth);
        case LQR_CMY_IMAGE:
            return 1. - lqr_pixel_get_norm(rgb, rgb_ind + channel, col_depth);
        case LQR_CMYK_IMAGE:
        case LQR_CMYKA_IMAGE:
            black_fact = 1 - lqr_pixel_get_norm(rgb, rgb_ind + 3, col_depth);
            return black_fact * (1. - (lqr_pixel_get_norm(rgb, rgb_ind + channel, col_depth)));
        case LQR_CUSTOM_IMAGE:
            return 0;
        default:
#ifdef __LQR_DEBUG__
            assert(0);
#endif /* __LQR_DEBUG__ */
            return 0;
    }
}

double
lqr_carver_read_brightness_grey(LqrCarver *r, int x, int y)
{
    int now = r->raw[y][x];
    int rgb_ind = now * r->channels;
    return lqr_pixel_get_norm(r->rgb, rgb_ind, r->col_depth);
}

double
lqr_carver_read_brightness_std(LqrCarver *r, int x, int y)
{
    double red, green, blue;
    int now = r->raw[y][x];
    int rgb_ind = now * r->channels;

    red = lqr_pixel_get_rgbcol(r->rgb, rgb_ind, r->col_depth, r->image_type, 0);
    green = lqr_pixel_get_rgbcol(r->rgb, rgb_ind, r->col_depth, r->image_type, 1);
    blue = lqr_pixel_get_rgbcol(r->rgb, rgb_ind, r->col_depth, r->image_type, 2);
    return (red + green + blue) / 3;
}

double
lqr_carver_read_brightness_custom(LqrCarver *r, int x, int y)
{
    double sum = 0;
    int k;
    int has_alpha = (r->alpha_channel >= 0 ? 1 : 0);
    int has_black = (r->black_channel >= 0 ? 1 : 0);
    uint32_t col_channels = r->channels - has_alpha - has_black;

    double black_fact = 0;

    int now = r->raw[y][x];

    if (has_black) {
        black_fact = lqr_pixel_get_norm(r->rgb, now * r->channels + r->black_channel, r->col_depth);
    }

    for (k = 0; k < r->channels; k++) {
        if ((k != r->alpha_channel) && (k != r->black_channel)) {
            double col = lqr_pixel_get_norm(r->rgb, now * r->channels + k, r->col_depth);
            sum += 1. - (1. - col) * (1. - black_fact);
        }
    }

    sum /= col_channels;

    if (has_black) {
        sum = 1 - sum;
    }

    return sum;
}

/* read average pixel value at x, y
 * for energy computation */
double
lqr_carver_read_brightness(LqrCarver *r, int x, int y)
{
    int has_alpha = (r->alpha_channel >= 0 ? 1 : 0);
    double alpha_fact = 1;

    int now = r->raw[y][x];

    double bright = 0;

    switch (r->image_type) {
        case LQR_GREY_IMAGE:
        case LQR_GREYA_IMAGE:
            bright = lqr_carver_read_brightness_grey(r, x, y);
            break;
        case LQR_RGB_IMAGE:
        case LQR_RGBA_IMAGE:
        case LQR_CMY_IMAGE:
        case LQR_CMYK_IMAGE:
        case LQR_CMYKA_IMAGE:
            bright = lqr_carver_read_brightness_std(r, x, y);
            break;
        case LQR_CUSTOM_IMAGE:
            bright = lqr_carver_read_brightness_custom(r, x, y);
            break;
    }

    if (has_alpha) {
        alpha_fact = lqr_pixel_get_norm(r->rgb, now * r->channels + r->alpha_channel, r->col_depth);
    }

    return bright * alpha_fact;
}

double
lqr_carver_read_luma_std(LqrCarver *r, int x, int y)
{
    double red, green, blue;
    int now = r->raw[y][x];
    int rgb_ind = now * r->channels;

    red = lqr_pixel_get_rgbcol(r->rgb, rgb_ind, r->col_depth, r->image_type, 0);
    green = lqr_pixel_get_rgbcol(r->rgb, rgb_ind, r->col_depth, r->image_type, 1);
    blue = lqr_pixel_get_rgbcol(r->rgb, rgb_ind, r->col_depth, r->image_type, 2);
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

double
lqr_carver_read_luma(LqrCarver *r, int x, int y)
{
    int has_alpha = (r->alpha_channel >= 0 ? 1 : 0);
    double alpha_fact = 1;

    int now = r->raw[y][x];

    double bright = 0;

    switch (r->image_type) {
        case LQR_GREY_IMAGE:
        case LQR_GREYA_IMAGE:
            bright = lqr_carver_read_brightness_grey(r, x, y);
            break;
        case LQR_RGB_IMAGE:
        case LQR_RGBA_IMAGE:
        case LQR_CMY_IMAGE:
        case LQR_CMYK_IMAGE:
        case LQR_CMYKA_IMAGE:
            bright = lqr_carver_read_luma_std(r, x, y);
            break;
        case LQR_CUSTOM_IMAGE:
            bright = lqr_carver_read_brightness_custom(r, x, y);
            break;
    }

    if (has_alpha) {
        alpha_fact = lqr_pixel_get_norm(r->rgb, now * r->channels + r->alpha_channel, r->col_depth);
    }

    return bright * alpha_fact;
}

double
lqr_carver_read_rgba(LqrCarver *r, int x, int y, int channel)
{
    int has_alpha = (r->alpha_channel >= 0 ? 1 : 0);

    int now = r->raw[y][x];

#ifdef __LQR_DEBUG__
    assert(channel >= 0 && channel < 4);
#endif /* __LQR_DEBUG__ */

    if (channel < 3) {
        switch (r->image_type) {
            case LQR_GREY_IMAGE:
            case LQR_GREYA_IMAGE:
                return lqr_carver_read_brightness_grey(r, x, y);
            case LQR_RGB_IMAGE:
            case LQR_RGBA_IMAGE:
            case LQR_CMY_IMAGE:
            case LQR_CMYK_IMAGE:
            case LQR_CMYKA_IMAGE:
                return lqr_pixel_get_rgbcol(r->rgb, now * r->channels, r->col_depth, r->image_type, channel);
            case LQR_CUSTOM_IMAGE:
                return 0;
            default:
#ifdef __LQR_DEBUG__
                assert(0);
#endif /* __LQR_DEBUG__ */
                return 0;
        }
    } else if (has_alpha) {
        return lqr_pixel_get_norm(r->rgb, now * r->channels + r->alpha_channel, r->col_depth);
    } else {
        return 1;
    }
}

double
lqr_carver_read_custom(LqrCarver *r, int x, int y, int channel)
{
    int now = r->raw[y][x];

    return lqr_pixel_get_norm(r->rgb, now * r->channels + channel, r->col_depth);
}

double
lqr_carver_read_cached_std(LqrCarver *r, int x, int y)
{
    int z0 = r->raw[y][x];

    return r->rcache[z0];
}

double
lqr_carver_read_cached_rgba(LqrCarver *r, int x, int y, int channel)
{
    int z0 = r->raw[y][x];

    return r->rcache[z0 * 4 + channel];
}

double
lqr_carver_read_cached_custom(LqrCarver *r, int x, int y, int channel)
{
    int z0 = r->raw[y][x];

    return r->rcache[z0 * r->channels + channel];
}

float
lqr_energy_builtin_grad_all(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow, LqrGradFunc gf)
{
    double gx, gy;

    double (*bread_func) (LqrReadingWindow *, int, int);

    switch (lqr_rwindow_get_read_t(rwindow)) {
        case LQR_ER_BRIGHTNESS:
            bread_func = lqr_rwindow_read_bright;
            break;
        case LQR_ER_LUMA:
            bread_func = lqr_rwindow_read_luma;
            break;
        default:
#ifdef __LQR_DEBUG__
            assert(0);
#endif /* __LQR_DEBUG__ */
            return 0;
    }

    if (y == 0) {
        gy = bread_func(rwindow, 0, 1) - bread_func(rwindow, 0, 0);
    } else if (y < img_height - 1) {
        gy = (bread_func(rwindow, 0, 1) - bread_func(rwindow, 0, -1)) / 2;
    } else {
        gy = bread_func(rwindow, 0, 0) - bread_func(rwindow, 0, -1);
    }

    if (x == 0) {
        gx = bread_func(rwindow, 1, 0) - bread_func(rwindow, 0, 0);
    } else if (x < img_width - 1) {
        gx = (bread_func(rwindow, 1, 0) - bread_func(rwindow, -1, 0)) / 2;
    } else {
        gx = bread_func(rwindow, 0, 0) - bread_func(rwindow, -1, 0);
    }

    return gf(gx, gy);
}

float
lqr_energy_builtin_grad_norm(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow,
                             void * extra_data)
{
    return lqr_energy_builtin_grad_all(x, y, img_width, img_height, rwindow, lqr_grad_norm);
}

float
lqr_energy_builtin_grad_sumabs(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow,
                               void * extra_data)
{
    return lqr_energy_builtin_grad_all(x, y, img_width, img_height, rwindow, lqr_grad_sumabs);
}

float
lqr_energy_builtin_grad_xabs(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow,
                             void * extra_data)
{
    return lqr_energy_builtin_grad_all(x, y, img_width, img_height, rwindow, lqr_grad_xabs);
}

float
lqr_energy_builtin_null(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow, void * extra_data)
{
    return 0;
}

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_set_energy_function_builtin(LqrCarver *r, LqrEnergyFuncBuiltinType ef_ind)
{
    switch (ef_ind) {
        case LQR_EF_GRAD_NORM:
            LQR_CATCH(lqr_carver_set_energy_function(r, lqr_energy_builtin_grad_norm, 1, LQR_ER_BRIGHTNESS, NULL));
            break;
        case LQR_EF_GRAD_SUMABS:
            LQR_CATCH(lqr_carver_set_energy_function(r, lqr_energy_builtin_grad_sumabs, 1, LQR_ER_BRIGHTNESS, NULL));
            break;
        case LQR_EF_GRAD_XABS:
            LQR_CATCH(lqr_carver_set_energy_function(r, lqr_energy_builtin_grad_xabs, 1, LQR_ER_BRIGHTNESS, NULL));
            break;
        case LQR_EF_LUMA_GRAD_NORM:
            LQR_CATCH(lqr_carver_set_energy_function(r, lqr_energy_builtin_grad_norm, 1, LQR_ER_LUMA, NULL));
            break;
        case LQR_EF_LUMA_GRAD_SUMABS:
            LQR_CATCH(lqr_carver_set_energy_function(r, lqr_energy_builtin_grad_sumabs, 1, LQR_ER_LUMA, NULL));
            break;
        case LQR_EF_LUMA_GRAD_XABS:
            LQR_CATCH(lqr_carver_set_energy_function(r, lqr_energy_builtin_grad_xabs, 1, LQR_ER_LUMA, NULL));
            break;
        case LQR_EF_NULL:
            LQR_CATCH(lqr_carver_set_energy_function(r, lqr_energy_builtin_null, 0, LQR_ER_BRIGHTNESS, NULL));
            break;
        default:
            return LQR_ERROR;
    }

    return LQR_OK;
}

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_set_energy_function(LqrCarver *r, LqrEnergyFunc en_func, int radius,
                               LqrEnergyReaderType reader_type, void * extra_data)
{
    LQR_CATCH_F(r->root == NULL);

    r->nrg = en_func;
    r->nrg_radius = radius;
    r->nrg_read_t = reader_type;
    r->nrg_extra_data = extra_data;

    LRQ_FREE(r->rcache);
    r->rcache = NULL;
    r->nrg_uptodate = false;

    lqr_rwindow_destroy(r->rwindow);

    if (reader_type == LQR_ER_CUSTOM) {
        LQR_CATCH_MEM(r->rwindow = lqr_rwindow_new_custom(radius, r->use_rcache, r->channels));
    } else {
        LQR_CATCH_MEM(r->rwindow = lqr_rwindow_new(radius, reader_type, r->use_rcache));
    }

    return LQR_OK;
}

double *
lqr_carver_generate_rcache_bright(LqrCarver *r)
{
    double *buffer;
    int x, y;
    int z0;

    LQR_TRY_N_N(buffer = LRQ_CALLOC(double, r->w0 * r->h0));

    for (y = 0; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            z0 = r->raw[y][x];
            buffer[z0] = lqr_carver_read_brightness(r, x, y);
        }
    }

    return buffer;
}

double *
lqr_carver_generate_rcache_luma(LqrCarver *r)
{
    double *buffer;
    int x, y;
    int z0;

    LQR_TRY_N_N(buffer = LRQ_CALLOC(double, r->w0 * r->h0));

    for (y = 0; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            z0 = r->raw[y][x];
            buffer[z0] = lqr_carver_read_luma(r, x, y);
        }
    }

    return buffer;
}

double *
lqr_carver_generate_rcache_rgba(LqrCarver *r)
{
    double *buffer;
    int x, y, k;
    int z0;

    LQR_TRY_N_N(buffer = LRQ_CALLOC(double, r->w0 * r->h0 * 4));

    for (y = 0; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            z0 = r->raw[y][x];
            for (k = 0; k < 4; k++) {
                buffer[z0 * 4 + k] = lqr_carver_read_rgba(r, x, y, k);
            }
        }
    }

    return buffer;
}

double *
lqr_carver_generate_rcache_custom(LqrCarver *r)
{
    double *buffer;
    int x, y, k;
    int z0;

    LQR_TRY_N_N(buffer = LRQ_CALLOC(double, r->w0 * r->h0 * r->channels));

    for (y = 0; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            z0 = r->raw[y][x];
            for (k = 0; k < r->channels; k++) {
                buffer[z0 * r->channels + k] = lqr_carver_read_custom(r, x, y, k);
            }
        }
    }

    return buffer;
}

double *
lqr_carver_generate_rcache(LqrCarver *r)
{
#ifdef __LQR_DEBUG__
    assert(r->w == r->w_start - r->max_level + 1);
#endif /* __LQR_DEBUG__ */

    switch (r->nrg_read_t) {
        case LQR_ER_BRIGHTNESS:
            return lqr_carver_generate_rcache_bright(r);
        case LQR_ER_LUMA:
            return lqr_carver_generate_rcache_luma(r);
        case LQR_ER_RGBA:
            return lqr_carver_generate_rcache_rgba(r);
        case LQR_ER_CUSTOM:
            return lqr_carver_generate_rcache_custom(r);
        default:
#ifdef __LQR_DEBUG__
            assert(0);
#endif /* __LQR_DEBUG__ */
            return NULL;
    }
}

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_get_energy(LqrCarver *r, float *buffer, int orientation)
{
    int x, y;
    int z0 = 0;
    int w, h;
    int buf_size;
    int data;
    float nrg;
    float nrg_min = FLT_MAX;
    float nrg_max = 0;

    LQR_CATCH_F(orientation == 0 || orientation == 1);
    LQR_CATCH_CANC(r);
    LQR_CATCH_F(buffer != NULL);

    if (r->nrg_active == false) {
        LQR_CATCH(lqr_carver_init_energy_related(r));
    }

    if (r->w != r->w_start - r->max_level + 1) {
#ifdef __LQR_DEBUG__
        assert(r->active);
#endif /* __LQR_DEBUG__ */
        LQR_CATCH(lqr_carver_flatten(r));
    }

    buf_size = r->w * r->h;

    if (orientation != lqr_carver_get_orientation(r)) {
        LQR_CATCH(lqr_carver_transpose(r));
    }
    LQR_CATCH(lqr_carver_build_emap(r));

    w = lqr_carver_get_width(r);
    h = lqr_carver_get_height(r);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            data = orientation == 0 ? r->raw[y][x] : r->raw[x][y];
            /* nrg = tanhf(r->en[data]); */
            nrg = LQR_SATURATE(r->en[data]);
            nrg_max = MAX(nrg_max, nrg);
            nrg_min = MIN(nrg_min, nrg);
            buffer[z0++] = nrg;
        }
    }

    if (nrg_max > nrg_min) {
        for (z0 = 0; z0 < buf_size; z0++) {
            buffer[z0] = (buffer[z0] - nrg_min) / (nrg_max - nrg_min);
        }
    }

    return LQR_OK;
}

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_get_true_energy(LqrCarver *r, float *buffer, int orientation)
{
    int x, y;
    int z0 = 0;
    int w, h;
    int data;

    LQR_CATCH_F(orientation == 0 || orientation == 1);
    LQR_CATCH_CANC(r);
    LQR_CATCH_F(buffer != NULL);

    if (r->nrg_active == false) {
        LQR_CATCH(lqr_carver_init_energy_related(r));
    }

    if (r->w != r->w_start - r->max_level + 1) {
#ifdef __LQR_DEBUG__
        assert(r->active);
#endif /* __LQR_DEBUG__ */
        LQR_CATCH(lqr_carver_flatten(r));
    }

    if (orientation != lqr_carver_get_orientation(r)) {
        LQR_CATCH(lqr_carver_transpose(r));
    }
    LQR_CATCH(lqr_carver_build_emap(r));

    w = lqr_carver_get_width(r);
    h = lqr_carver_get_height(r);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            data = orientation == 0 ? r->raw[y][x] : r->raw[x][y];
            /* nrg = tanhf(r->en[data]); */
            buffer[z0++] = r->en[data];
        }
    }

    return LQR_OK;
}

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_get_energy_image(LqrCarver *r, void *buffer, int orientation, LqrColDepth col_depth,
                            LqrImageType image_type)
{
    int x, y;
    int z0 = 0;
    int w, h;
    int buf_size;
    int data;
    float nrg;
    float nrg_min = FLT_MAX;
    float nrg_max = 0;
    float *aux_buffer;
    int k;
    int channels;
    int alpha_channel, black_channel;
    bool has_alpha, has_black, col_model_is_additive;

    LQR_CATCH_F(orientation == 0 || orientation == 1);
    LQR_CATCH_CANC(r);
    LQR_CATCH_F(buffer != NULL);

    switch (image_type) {
        case LQR_GREY_IMAGE:
            channels = 1;
            alpha_channel = -1;
            black_channel = -1;
            col_model_is_additive = true;
            break;
        case LQR_GREYA_IMAGE:
            channels = 2;
            alpha_channel = 1;
            black_channel = -1;
            col_model_is_additive = true;
            break;
        case LQR_RGB_IMAGE:
            channels = 3;
            alpha_channel = -1;
            black_channel = -1;
            col_model_is_additive = true;
            break;
        case LQR_RGBA_IMAGE:
            channels = 4;
            alpha_channel = 3;
            black_channel = -1;
            col_model_is_additive = true;
            break;
        case LQR_CMY_IMAGE:
            channels = 3;
            alpha_channel = -1;
            black_channel = -1;
            col_model_is_additive = false;
            break;
        case LQR_CMYK_IMAGE:
            channels = 4;
            alpha_channel = -1;
            black_channel = 3;
            col_model_is_additive = false;
            break;
        case LQR_CMYKA_IMAGE:
            channels = 5;
            alpha_channel = 4;
            black_channel = 3;
            col_model_is_additive = false;
            break;
        case LQR_CUSTOM_IMAGE:
        default:
            return LQR_ERROR;
    }

    has_alpha = (alpha_channel >= 0 ? true : false);
    has_black = (black_channel >= 0 ? true : false);

    if (r->nrg_active == false) {
        LQR_CATCH(lqr_carver_init_energy_related(r));
    }

    if (r->w != r->w_start - r->max_level + 1) {
#ifdef __LQR_DEBUG__
        assert(r->active);
#endif /* __LQR_DEBUG__ */
        LQR_CATCH(lqr_carver_flatten(r));
    }

    buf_size = r->w * r->h;

    LQR_CATCH_MEM(aux_buffer = LRQ_CALLOC(float, buf_size));

    if (orientation != lqr_carver_get_orientation(r)) {
        LQR_CATCH(lqr_carver_transpose(r));
    }
    LQR_CATCH(lqr_carver_build_emap(r));

    w = lqr_carver_get_width(r);
    h = lqr_carver_get_height(r);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            data = orientation == 0 ? r->raw[y][x] : r->raw[x][y];
            /* nrg = tanhf(r->en[data]); */
            nrg = LQR_SATURATE(r->en[data]);
            nrg_max = MAX(nrg_max, nrg);
            nrg_min = MIN(nrg_min, nrg);
            aux_buffer[z0++] = nrg;
        }
    }

    for (z0 = 0; z0 < buf_size; z0++) {
        if (nrg_max > nrg_min) {
            nrg = (aux_buffer[z0] - nrg_min) / (nrg_max - nrg_min);
        } else {
            nrg = 0;
        }
        if (col_model_is_additive) {
            for (k = 0; k < channels; k++) {
                if (k != alpha_channel) {
                    lqr_pixel_set_norm(nrg, buffer, z0 * channels + k, col_depth);
                }
            }
        } else {
            nrg = 1 - nrg;
            if (has_black) {
                lqr_pixel_set_norm(nrg, buffer, z0 * channels + black_channel, col_depth);
                for (k = 0; k < channels; k++) {
                    if ((k != alpha_channel) && (k != black_channel)) {
                        lqr_pixel_set_norm(0.0, buffer, z0 * channels + k, col_depth);
                    }
                }
            } else {
                for (k = 0; k < channels; k++) {
                    if ((k != alpha_channel) && (k != black_channel)) {
                        lqr_pixel_set_norm(nrg, buffer, z0 * channels + k, col_depth);
                    }
                }
            }
        }
        if (has_alpha) {
            lqr_pixel_set_norm(1.0, buffer, z0 * channels + alpha_channel, col_depth);
        }
    }

    LRQ_FREE(aux_buffer);

    return LQR_OK;
}
