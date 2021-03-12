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

#include "lqr_all.h"

#ifdef __LQR_DEBUG__
#include <stdio.h>
#include <assert.h>
#endif /* __LQR_DEBUG__ */

LqrRetVal
lqr_rwindow_fill_std(LqrReadingWindow *rwindow, LqrCarver *r, int x, int y)
{
    double **buffer;
    int i, j;

    LqrReadFunc read_float;

    buffer = rwindow->buffer;

    switch (rwindow->read_t) {
        case LQR_ER_BRIGHTNESS:
            read_float = lqr_carver_read_brightness;
            break;
        case LQR_ER_LUMA:
            read_float = lqr_carver_read_luma;
            break;
        default:
#ifdef __LQR_DEBUG__
            assert(0);
#endif /* __LQR_DEBUG__ */
            return LQR_ERROR;
    }

    for (i = -rwindow->radius; i <= rwindow->radius; i++) {
        for (j = -rwindow->radius; j <= rwindow->radius; j++) {
            if (x + i < 0 || x + i >= r->w || y + j < 0 || y + j >= r->h) {
                buffer[i][j] = 0;
            } else {
                buffer[i][j] = read_float(r, x + i, y + j);
            }
        }
    }

    return LQR_OK;
}

LqrRetVal
lqr_rwindow_fill_rgba(LqrReadingWindow *rwindow, LqrCarver *r, int x, int y)
{
    double **buffer;
    int i, j, k;

    buffer = rwindow->buffer;

    LQR_CATCH_F(lqr_rwindow_get_read_t(rwindow) == LQR_ER_RGBA);

    for (i = -rwindow->radius; i <= rwindow->radius; i++) {
        for (j = -rwindow->radius; j <= rwindow->radius; j++) {
            if (x + i < 0 || x + i >= r->w || y + j < 0 || y + j >= r->h) {
                for (k = 0; k < 4; k++) {
                    buffer[i][4 * j + k] = 0;
                }
            } else {
                for (k = 0; k < 4; k++) {
                    buffer[i][4 * j + k] = lqr_carver_read_rgba(r, x + i, y + j, k);
                }
            }
        }
    }

    return LQR_OK;
}

LqrRetVal
lqr_rwindow_fill_custom(LqrReadingWindow *rwindow, LqrCarver *r, int x, int y)
{
    double **buffer;
    int i, j, k;

    buffer = rwindow->buffer;

    LQR_CATCH_F(lqr_rwindow_get_read_t(rwindow) == LQR_ER_CUSTOM);

    for (i = -rwindow->radius; i <= rwindow->radius; i++) {
        for (j = -rwindow->radius; j <= rwindow->radius; j++) {
            if (x + i < 0 || x + i >= r->w || y + j < 0 || y + j >= r->h) {
                for (k = 0; k < r->channels; k++) {
                    buffer[i][r->channels * j + k] = 0;
                }
            } else {
                for (k = 0; k < r->channels; k++) {
                    buffer[i][r->channels * j + k] = lqr_carver_read_custom(r, x + i, y + j, k);
                }
            }
        }
    }

    return LQR_OK;
}

LqrRetVal
lqr_rwindow_fill(LqrReadingWindow *rwindow, LqrCarver *r, int x, int y)
{
    LQR_CATCH_CANC(r);

    rwindow->carver = r;
    rwindow->x = x;
    rwindow->y = y;

    if (rwindow->use_rcache) {
        return LQR_OK;
    }

    switch (rwindow->read_t) {
        case LQR_ER_BRIGHTNESS:
        case LQR_ER_LUMA:
            LQR_CATCH(lqr_rwindow_fill_std(rwindow, r, x, y));
            break;
        case LQR_ER_RGBA:
            LQR_CATCH(lqr_rwindow_fill_rgba(rwindow, r, x, y));
            break;
        case LQR_ER_CUSTOM:
            LQR_CATCH(lqr_rwindow_fill_custom(rwindow, r, x, y));
            break;
        default:
            return LQR_ERROR;
    }
    return LQR_OK;
}

LqrReadingWindow *
lqr_rwindow_new_std(int radius, LqrEnergyReaderType read_func_type, bool use_rcache)
{
    LqrReadingWindow *out_rwindow;
    double **out_buffer;
    double *out_buffer_aux;

    int buf_size1, buf_size2;
    int i;

    LQR_TRY_N_N(out_rwindow = LRQ_CALLOC(LqrReadingWindow, 1));

    buf_size1 = (2 * radius + 1);
    buf_size2 = buf_size1 * buf_size1;

    LQR_TRY_N_N(out_buffer_aux = LRQ_CALLOC(double, buf_size2));
    LQR_TRY_N_N(out_buffer = LRQ_CALLOC(double *, buf_size1));
    for (i = 0; i < buf_size1; i++) {
        out_buffer[i] = out_buffer_aux + radius;
        out_buffer_aux += buf_size1;
    }
    out_buffer += radius;

    out_rwindow->buffer = out_buffer;
    out_rwindow->radius = radius;
    out_rwindow->read_t = read_func_type;
    out_rwindow->channels = 1;
    out_rwindow->use_rcache = use_rcache;
    out_rwindow->carver = NULL;
    out_rwindow->x = 0;
    out_rwindow->y = 0;

    return out_rwindow;
}

LqrReadingWindow *
lqr_rwindow_new_rgba(int radius, bool use_rcache)
{
    LqrReadingWindow *out_rwindow;
    double **out_buffer;
    double *out_buffer_aux;

    int buf_size1, buf_size2;
    int i;

    LQR_TRY_N_N(out_rwindow = LRQ_CALLOC(LqrReadingWindow, 1));

    buf_size1 = (2 * radius + 1);
    buf_size2 = buf_size1 * buf_size1 * 4;

    LQR_TRY_N_N(out_buffer_aux = LRQ_CALLOC(double, buf_size2));
    LQR_TRY_N_N(out_buffer = LRQ_CALLOC(double *, buf_size1));
    for (i = 0; i < buf_size1; i++) {
        out_buffer[i] = out_buffer_aux + radius * 4;
        out_buffer_aux += buf_size1 * 4;
    }
    out_buffer += radius;

    out_rwindow->buffer = out_buffer;
    out_rwindow->radius = radius;
    out_rwindow->read_t = LQR_ER_RGBA;
    out_rwindow->channels = 4;
    out_rwindow->use_rcache = use_rcache;
    out_rwindow->carver = NULL;
    out_rwindow->x = 0;
    out_rwindow->y = 0;

    return out_rwindow;
}

LqrReadingWindow *
lqr_rwindow_new_custom(int radius, bool use_rcache, int channels)
{
    LqrReadingWindow *out_rwindow;
    double **out_buffer;
    double *out_buffer_aux;

    int buf_size1, buf_size2;
    int i;

    LQR_TRY_N_N(out_rwindow = LRQ_CALLOC(LqrReadingWindow, 1));

    buf_size1 = (2 * radius + 1);
    buf_size2 = buf_size1 * buf_size1 * channels;

    LQR_TRY_N_N(out_buffer_aux = LRQ_CALLOC(double, buf_size2));
    LQR_TRY_N_N(out_buffer = LRQ_CALLOC(double *, buf_size1));
    for (i = 0; i < buf_size1; i++) {
        out_buffer[i] = out_buffer_aux + radius * channels;
        out_buffer_aux += buf_size1 * channels;
    }
    out_buffer += radius;

    out_rwindow->buffer = NULL;
    out_rwindow->radius = radius;
    out_rwindow->read_t = LQR_ER_CUSTOM;
    out_rwindow->channels = channels;
    out_rwindow->use_rcache = use_rcache;
    out_rwindow->carver = NULL;
    out_rwindow->x = 0;
    out_rwindow->y = 0;

    return out_rwindow;
}

LqrReadingWindow *
lqr_rwindow_new(int radius, LqrEnergyReaderType read_func_type, bool use_rcache)
{
    switch (read_func_type) {
        case LQR_ER_BRIGHTNESS:
        case LQR_ER_LUMA:
            return lqr_rwindow_new_std(radius, read_func_type, use_rcache);
        case LQR_ER_RGBA:
            return lqr_rwindow_new_rgba(radius, use_rcache);
        case LQR_ER_CUSTOM:
        default:
#ifdef __LQR_DEBUG__
            assert(0);
#endif /* __LQR_DEBUG__ */
            return NULL;
    }
}

void
lqr_rwindow_destroy(LqrReadingWindow *rwindow)
{
    double **buffer;

    if (rwindow == NULL) {
        return;
    }

    if (rwindow->buffer == NULL) {
        return;
    }

    buffer = rwindow->buffer;
    buffer -= rwindow->radius;
    buffer[0] -= rwindow->radius * rwindow->channels;
    LRQ_FREE(buffer[0]);
    LRQ_FREE(buffer);
    LRQ_FREE(rwindow);
}

double
lqr_rwindow_read_bright(LqrReadingWindow *rwindow, int x, int y)
{
    if (rwindow->use_rcache) {
        return lqr_carver_read_cached_std(rwindow->carver, rwindow->x + x, rwindow->y + y);
    }

    return rwindow->buffer[x][y];
}

double
lqr_rwindow_read_luma(LqrReadingWindow *rwindow, int x, int y)
{
    if (rwindow->use_rcache) {
        return lqr_carver_read_cached_std(rwindow->carver, rwindow->x + x, rwindow->y + y);
    }

    return rwindow->buffer[x][y];
}

double
lqr_rwindow_read_rgba(LqrReadingWindow *rwindow, int x, int y, int channel)
{
    if (rwindow->use_rcache) {
        return lqr_carver_read_cached_rgba(rwindow->carver, rwindow->x + x, rwindow->y + y, channel);
    }

    return rwindow->buffer[x][4 * y + channel];
}

double
lqr_rwindow_read_custom(LqrReadingWindow *rwindow, int x, int y, int channel)
{
    if (rwindow->use_rcache) {
        return lqr_carver_read_cached_custom(rwindow->carver, rwindow->x + x, rwindow->y + y, channel);
    }

    return rwindow->buffer[x][rwindow->channels * y + channel];
}

/* LQR_PUBLIC */
double
lqr_rwindow_read(LqrReadingWindow *rwindow, int x, int y, int channel)
{
    int x1, y1;

#ifdef __LQR_DEBUG__
    assert(rwindow != NULL);
#endif /* __LQR_DEBUG__ */

    x1 = rwindow->x + x;
    y1 = rwindow->y + y;

    if (x < -rwindow->radius || x > rwindow->radius ||
        y < -rwindow->radius || y > rwindow->radius ||
        x1 < 0 || x1 >= rwindow->carver->w || y1 < 0 || y1 >= rwindow->carver->h) {
        return 0;
    }

    switch (rwindow->read_t) {
        case LQR_ER_BRIGHTNESS:
            return lqr_rwindow_read_bright(rwindow, x, y);
        case LQR_ER_LUMA:
            return lqr_rwindow_read_luma(rwindow, x, y);
        case LQR_ER_RGBA:
            return lqr_rwindow_read_rgba(rwindow, x, y, channel);
        case LQR_ER_CUSTOM:
            return lqr_rwindow_read_custom(rwindow, x, y, channel);
        default:
#ifdef __LQR_DEBUG__
            assert(0);
#endif /* __LQR_DEBUG__ */
            return 0;
    }
}

/* LQR_PUBLIC */
LqrEnergyReaderType
lqr_rwindow_get_read_t(LqrReadingWindow *rwindow)
{
    return rwindow->read_t;
}

/* LQR_PUBLIC */
int
lqr_rwindow_get_radius(LqrReadingWindow *rwindow)
{
    return rwindow->radius;
}

/* LQR_PUBLIC */
int
lqr_rwindow_get_channels(LqrReadingWindow *rwindow)
{
    return rwindow->channels;
}
