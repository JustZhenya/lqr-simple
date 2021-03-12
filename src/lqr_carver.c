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

#include <math.h>
#include <unistd.h>

#include "lqr_all.h"

#ifdef __LQR_VERBOSE__
#include <stdio.h>
#endif /* __LQR_VERBOSE__ */

#ifdef __LQR_DEBUG__
#include <stdio.h>
#include <assert.h>
#endif /* __LQR_DEBUG__ */

/**** LQR_CARVER CLASS FUNCTIONS ****/

/*** constructor & destructor ***/

/* constructors */
LqrCarver *
lqr_carver_new_common(int width, int height, int channels)
{
    LqrCarver *r;

    LQR_TRY_N_N(r = LRQ_CALLOC(LqrCarver, 1));

    atomic_init(&r->state, LQR_CARVER_STATE_STD);
    atomic_init(&r->state_lock, 0);
    atomic_init(&r->state_lock_queue, 0);

    r->level = 1;
    r->max_level = 1;
    r->transposed = 0;
    r->active = false;
    r->nrg_active = false;
    r->root = NULL;
    r->rigidity = 0;
    r->resize_aux_layers = false;
    r->dump_vmaps = false;
    r->resize_order = LQR_RES_ORDER_HOR;
    r->attached_list = NULL;
    r->flushed_vs = NULL;
    r->preserve_in_buffer = false;
    LQR_TRY_N_N(r->progress = lqr_progress_new());
    r->session_update_step = 1;
    r->session_rescale_total = 0;
    r->session_rescale_current = 0;

    r->en = NULL;
    r->bias = NULL;
    r->m = NULL;
    r->least = NULL;
    r->_raw = NULL;
    r->raw = NULL;
    r->vpath = NULL;
    r->vpath_x = NULL;
    r->rigidity_map = NULL;
    r->rigidity_mask = NULL;
    r->delta_x = 1;

    r->h = height;
    r->w = width;
    r->channels = channels;

    r->w0 = r->w;
    r->h0 = r->h;
    r->w_start = r->w;
    r->h_start = r->h;

    r->rcache = NULL;
    r->use_rcache = true;

    r->rwindow = NULL;
    lqr_carver_set_energy_function_builtin(r, LQR_EF_GRAD_XABS);
    r->nrg_xmin = NULL;
    r->nrg_xmax = NULL;
    r->nrg_uptodate = false;

    r->leftright = 0;
    r->lr_switch_frequency = 0;

    r->enl_step = 2.0;

    LQR_TRY_N_N(r->vs = LRQ_CALLOC(int, r->w * r->h));

    /* initialize cursor */

    LQR_TRY_N_N(r->c = lqr_cursor_create(r));

    switch (channels) {
        case 1:
            lqr_carver_set_image_type(r, LQR_GREY_IMAGE);
            break;
        case 2:
            lqr_carver_set_image_type(r, LQR_GREYA_IMAGE);
            break;
        case 3:
            lqr_carver_set_image_type(r, LQR_RGB_IMAGE);
            break;
        case 4:
            lqr_carver_set_image_type(r, LQR_RGBA_IMAGE);
            break;
        case 5:
            lqr_carver_set_image_type(r, LQR_CMYKA_IMAGE);
            break;
        default:
            lqr_carver_set_image_type(r, LQR_CUSTOM_IMAGE);
            break;
    }

    return r;
}

/* LQR_PUBLIC */
LqrCarver *
lqr_carver_new(uint8_t *buffer, int width, int height, int channels)
{
    return lqr_carver_new_ext(buffer, width, height, channels, LQR_COLDEPTH_8I);
}

/* LQR_PUBLIC */
LqrCarver *
lqr_carver_new_ext(void *buffer, int width, int height, int channels, LqrColDepth colour_depth)
{
    LqrCarver *r;

    LQR_TRY_N_N(r = lqr_carver_new_common(width, height, channels));

    r->rgb = (void *) buffer;

    BUF_TRY_NEW_RET_POINTER(r->rgb_ro_buffer, r->channels * r->w, colour_depth);

    r->col_depth = colour_depth;

    return r;
}

/* destructor */
/* LQR_PUBLIC */
void
lqr_carver_destroy(LqrCarver *r)
{
    if (!r->preserve_in_buffer) {
        LRQ_FREE(r->rgb);
    }
    if (r->root == NULL) {
        LRQ_FREE(r->vs);
    }
    LRQ_FREE(r->rgb_ro_buffer);
    LRQ_FREE(r->en);
    LRQ_FREE(r->bias);
    LRQ_FREE(r->m);
    LRQ_FREE(r->rcache);
    LRQ_FREE(r->least);
    lqr_cursor_destroy(r->c);
    LRQ_FREE(r->vpath);
    LRQ_FREE(r->vpath_x);
    if (r->rigidity_map != NULL) {
        r->rigidity_map -= r->delta_x;
        LRQ_FREE(r->rigidity_map);
    }
    LRQ_FREE(r->rigidity_mask);
    lqr_rwindow_destroy(r->rwindow);
    LRQ_FREE(r->nrg_xmin);
    LRQ_FREE(r->nrg_xmax);
    lqr_vmap_list_destroy(r->flushed_vs);
    lqr_carver_list_destroy(r->attached_list);
    LRQ_FREE(r->progress);
    LRQ_FREE(r->_raw);
    LRQ_FREE(r->raw);
    LRQ_FREE(r);
}

/*** initialization ***/

LqrRetVal
lqr_carver_init_energy_related(LqrCarver *r)
{
    int y, x;

    LQR_CATCH_F(r->active == false);
    LQR_CATCH_F(r->nrg_active == false);

    LQR_CATCH_MEM(r->en = LRQ_CALLOC(float, r->w * r->h));
    LQR_CATCH_MEM(r->_raw = LRQ_CALLOC(int, r->h_start * r->w_start));
    LQR_CATCH_MEM(r->raw = LRQ_CALLOC(int *, r->h_start));

    for (y = 0; y < r->h; y++) {
        r->raw[y] = r->_raw + y * r->w_start;
        for (x = 0; x < r->w_start; x++) {
            r->raw[y][x] = y * r->w_start + x;
        }
    }

    r->nrg_active = true;

    return LQR_OK;
}

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_init(LqrCarver *r, int delta_x, float rigidity)
{
    int x;

    LQR_CATCH_CANC(r);

    LQR_CATCH_F(r->active == false);

    if (r->nrg_active == false) {
        LQR_CATCH(lqr_carver_init_energy_related(r));
    }

    /* LQR_CATCH_MEM (r->bias = LRQ_CALLOC (float, r->w * r->h)); */
    LQR_CATCH_MEM(r->m = LRQ_CALLOC(float, r->w * r->h));
    LQR_CATCH_MEM(r->least = LRQ_CALLOC(int, r->w * r->h));

    LQR_CATCH_MEM(r->vpath = LRQ_CALLOC(int, r->h));
    LQR_CATCH_MEM(r->vpath_x = LRQ_CALLOC(int, r->h));

    LQR_CATCH_MEM(r->nrg_xmin = LRQ_CALLOC(int, r->h));
    LQR_CATCH_MEM(r->nrg_xmax = LRQ_CALLOC(int, r->h));

    /* set rigidity map */
    r->delta_x = delta_x;
    r->rigidity = rigidity;

    r->rigidity_map = LRQ_CALLOC(float, 2 * r->delta_x + 1);
    r->rigidity_map += r->delta_x;
    for (x = -r->delta_x; x <= r->delta_x; x++) {
        r->rigidity_map[x] = r->rigidity * powf(fabsf(x), 1.5) / r->h;
    }

    r->active = true;

    return LQR_OK;
}

/*** set attributes ***/

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_set_image_type(LqrCarver *r, LqrImageType image_type)
{
    LQR_CATCH_CANC(r);

    switch (image_type) {
        case LQR_GREY_IMAGE:
            if (r->channels != 1) {
                return LQR_ERROR;
            }
            r->alpha_channel = -1;
            r->black_channel = -1;
            break;
        case LQR_GREYA_IMAGE:
            if (r->channels != 2) {
                return LQR_ERROR;
            }
            r->alpha_channel = 1;
            r->black_channel = -1;
            break;
        case LQR_CMY_IMAGE:
        case LQR_RGB_IMAGE:
            if (r->channels != 3) {
                return LQR_ERROR;
            }
            r->alpha_channel = -1;
            r->black_channel = -1;
            break;
        case LQR_CMYK_IMAGE:
            if (r->channels != 4) {
                return LQR_ERROR;
            }
            r->alpha_channel = -1;
            r->black_channel = 3;
            break;
        case LQR_RGBA_IMAGE:
            if (r->channels != 4) {
                return LQR_ERROR;
            }
            r->alpha_channel = 3;
            r->black_channel = -1;
            break;
        case LQR_CMYKA_IMAGE:
            if (r->channels != 5) {
                return LQR_ERROR;
            }
            r->alpha_channel = 4;
            r->black_channel = 3;
            break;
        case LQR_CUSTOM_IMAGE:
            r->alpha_channel = -1;
            r->black_channel = -1;
            break;
        default:
            return LQR_ERROR;
    }
    r->image_type = image_type;

    LRQ_FREE(r->rcache);
    r->rcache = NULL;
    r->nrg_uptodate = false;

    return LQR_OK;
}

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_set_alpha_channel(LqrCarver *r, int channel_index)
{
    bool changed = true;
    LQR_CATCH_CANC(r);

    if (channel_index < 0) {
        if (r->alpha_channel != -1) {
            r->alpha_channel = -1;
        } else {
            changed = false;
        }
    } else if (channel_index < r->channels) {
        if (r->alpha_channel != channel_index) {
            if (r->black_channel == channel_index) {
                r->black_channel = -1;
            }
            r->alpha_channel = channel_index;
        } else {
            changed = false;
        }
    } else {
        return LQR_ERROR;
    }

    if (r->image_type != LQR_CUSTOM_IMAGE) {
        r->image_type = LQR_CUSTOM_IMAGE;
        changed = true;
    }

    if (changed) {
        LRQ_FREE(r->rcache);
        r->rcache = NULL;
        r->nrg_uptodate = false;
    }

    return LQR_OK;
}

/* LQR_PUBLIC */
LqrRetVal
lqr_carver_set_black_channel(LqrCarver *r, int channel_index)
{
    bool changed = true;
    LQR_CATCH_CANC(r);

    if (channel_index < 0) {
        if (r->black_channel != -1) {
            r->black_channel = -1;
        } else {
            changed = false;
        }
    } else if (channel_index < r->channels) {
        if (r->black_channel != channel_index) {
            if (r->alpha_channel == channel_index) {
                r->alpha_channel = -1;
            }
            r->black_channel = channel_index;
        } else {
            changed = false;
        }
    } else {
        return LQR_ERROR;
    }

    if (r->image_type != LQR_CUSTOM_IMAGE) {
        r->image_type = LQR_CUSTOM_IMAGE;
        changed = true;
    }

    if (changed) {
        LRQ_FREE(r->rcache);
        r->rcache = NULL;
        r->nrg_uptodate = false;
    }

    return LQR_OK;
}

/* set gradient function */
/* WARNING: THIS FUNCTION IS ONLY MAINTAINED FOR BACK-COMPATIBILITY PURPOSES */
/* lqr_carver_set_energy_function_builtin() should be used in newly written code instead */
/* LQR_PUBLIC */
void
lqr_carver_set_gradient_function(LqrCarver *r, LqrGradFuncType gf_ind)
{
    switch (gf_ind) {
        case LQR_GF_NORM:
            lqr_carver_set_energy_function_builtin(r, LQR_EF_GRAD_NORM);
            break;
        case LQR_GF_SUMABS:
            lqr_carver_set_energy_function_builtin(r, LQR_EF_GRAD_SUMABS);
            break;
        case LQR_GF_XABS:
            lqr_carver_set_energy_function_builtin(r, LQR_EF_GRAD_XABS);
            break;
        case LQR_GF_NULL:
            lqr_carver_set_energy_function_builtin(r, LQR_EF_NULL);
            break;
        case LQR_GF_NORM_BIAS:
        case LQR_GF_YABS:
            lqr_carver_set_energy_function_builtin(r, LQR_EF_NULL);
            break;
#ifdef __LQR_DEBUG__
        default:
            assert(0);
#endif /* __LQR_DEBUG__ */
    }
}

/* attach carvers to be scaled along with the main one */
/* LQR_PUBLIC */
LqrRetVal
lqr_carver_attach(LqrCarver *r, LqrCarver *aux)
{
    LQR_CATCH_F(r->w0 == aux->w0);
    LQR_CATCH_F(r->h0 == aux->h0);
    LQR_CATCH_F(atomic_load(&r->state) == LQR_CARVER_STATE_STD);
    LQR_CATCH_F(atomic_load(&aux->state) == LQR_CARVER_STATE_STD);
    LQR_CATCH_MEM(r->attached_list = lqr_carver_list_append(r->attached_list, aux));
    LRQ_FREE(aux->vs);
    aux->vs = r->vs;
    aux->root = r;

    return LQR_OK;
}

/* set the seam output flag */
/* LQR_PUBLIC */
void
lqr_carver_set_dump_vmaps(LqrCarver *r)
{
    r->dump_vmaps = true;
}

/* unset the seam output flag */
/* LQR_PUBLIC */
void
lqr_carver_set_no_dump_vmaps(LqrCarver *r)
{
    r->dump_vmaps = false;
}

/* set order if rescaling in both directions */
/* LQR_PUBLIC */
void
lqr_carver_set_resize_order(LqrCarver *r, LqrResizeOrder resize_order)
{
    r->resize_order = resize_order;
}

/* set leftright switch interval */
/* LQR_PUBLIC */
void
lqr_carver_set_side_switch_frequency(LqrCarver *r, uint32_t switch_frequency)
{
    r->lr_switch_frequency = switch_frequency;
}

/* set enlargement step */
/* LQR_PUBLIC */
LqrRetVal
lqr_carver_set_enl_step(LqrCarver *r, float enl_step)
{
    LQR_CATCH_F((enl_step > 1) && (enl_step <= 2));
    LQR_CATCH_CANC(r);
    r->enl_step = enl_step;
    return LQR_OK;
}

/* LQR_PUBLIC */
void
lqr_carver_set_use_cache(LqrCarver *r, bool use_cache)
{
    if (!use_cache) {
        LRQ_FREE(r->rcache);
        r->rcache = NULL;
    }
    r->use_rcache = use_cache;
    r->rwindow->use_rcache = use_cache;
}

/* set progress reprot */
/* LQR_PUBLIC */
void
lqr_carver_set_progress(LqrCarver *r, LqrProgress * p)
{
    LRQ_FREE(r->progress);
    r->progress = p;
}

/* flag the input buffer to avoid destruction */
/* LQR_PUBLIC */
void
lqr_carver_set_preserve_input_image(LqrCarver *r)
{
    r->preserve_in_buffer = true;
}

/*** compute maps (energy, minpath & visibility) ***/

/* build multisize image up to given depth
 * it is progressive (can be called multilple times) */
LqrRetVal
lqr_carver_build_maps(LqrCarver *r, int depth)
{
#ifdef __LQR_DEBUG__
    assert(depth <= r->w_start);
    assert(depth >= 1);
#endif /* __LQR_DEBUG__ */

    LQR_CATCH_CANC(r);

    /* only go deeper if needed */
    if (depth > r->max_level) {
        LQR_CATCH_F(r->active);
        LQR_CATCH_F(r->root == NULL);

        /* set to minimum width reached so far */
        lqr_carver_set_width(r, r->w_start - r->max_level + 1);

        /* compute energy & minpath maps */
        LQR_CATCH(lqr_carver_build_emap(r));
        LQR_CATCH(lqr_carver_build_mmap(r));

        /* compute visibility map */
        LQR_CATCH(lqr_carver_build_vsmap(r, depth));
    }
    return LQR_OK;
}

/* compute energy map */
LqrRetVal
lqr_carver_build_emap(LqrCarver *r)
{
    int x, y;

    LQR_CATCH_CANC(r);

    if (r->nrg_uptodate) {
        return LQR_OK;
    }

    if (r->use_rcache && r->rcache == NULL) {
        LQR_CATCH_MEM(r->rcache = lqr_carver_generate_rcache(r));
    }

    for (y = 0; y < r->h; y++) {
        LQR_CATCH_CANC(r);
        /* r->nrg_xmin[y] = 0; */
        /* r->nrg_xmax[y] = r->w - 1; */
        for (x = 0; x < r->w; x++) {
            LQR_CATCH(lqr_carver_compute_e(r, x, y));
        }
    }

    r->nrg_uptodate = true;

    return LQR_OK;
}

LqrRetVal
lqr_carver_compute_e(LqrCarver *r, int x, int y)
{
    int data;
    float b_add = 0;

    /* removed CANC check for performance reasons */
    /* LQR_CATCH_CANC (r); */

    data = r->raw[y][x];

    LQR_CATCH(lqr_rwindow_fill(r->rwindow, r, x, y));
    if (r->bias != NULL) {
        b_add = r->bias[data] / r->w_start;
    }
    r->en[data] = r->nrg(x, y, r->w, r->h, r->rwindow, r->nrg_extra_data) + b_add;

    return LQR_OK;
}

/* compute auxiliary minpath map
 * defined as
 *   y = 1 : m(x,y) = e(x,y)
 *   y > 1 : m(x,y) = min_{x'=-dx,..,dx} ( m(x-x',y-1) + rig(x') ) + e(x,y)
 * where
 *   e(x,y)  is the energy at point (x,y)
 *   dx      is the max seam step delta_x
 *   rig(x') is the rigidity for step x'
 */
LqrRetVal
lqr_carver_build_mmap(LqrCarver *r)
{
    int x, y;
    int data;
    int data_down;
    int x1_min, x1_max, x1;
    float m, m1, r_fact;

    LQR_CATCH_CANC(r);

    /* span first row */
    for (x = 0; x < r->w; x++) {
        data = r->raw[0][x];
#ifdef __LQR_DEBUG__
        assert(r->vs[data] == 0);
#endif /* __LQR_DEBUG__ */
        r->m[data] = r->en[data];
    }

    /* span all other rows */
    for (y = 1; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            LQR_CATCH_CANC(r);

            data = r->raw[y][x];
#ifdef __LQR_DEBUG__
            assert(r->vs[data] == 0);
#endif /* __LQR_DEBUG__ */
            /* watch for boundaries */
            x1_min = MAX(-x, -r->delta_x);
            x1_max = MIN(r->w - 1 - x, r->delta_x);
            if (r->rigidity_mask) {
                r_fact = r->rigidity_mask[data];
            } else {
                r_fact = 1;
            }

            /* we use the data_down pointer to be able to
             * track the seams later (needed for rigidity) */
            data_down = r->raw[y - 1][x + x1_min];
            r->least[data] = data_down;
            if (r->rigidity) {
                m = r->m[data_down] + r_fact * r->rigidity_map[x1_min];
                for (x1 = x1_min + 1; x1 <= x1_max; x1++) {
                    data_down = r->raw[y - 1][x + x1];
                    /* find the min among the neighbors
                     * in the last row */
                    m1 = r->m[data_down] + r_fact * r->rigidity_map[x1];
                    if ((m1 < m) || ((m1 == m) && (r->leftright == 1))) {
                        m = m1;
                        r->least[data] = data_down;
                    }
                    /* m = MIN(m, r->m[data_down] + r->rigidity_map[x1]); */
                }
            } else {
                m = r->m[data_down];
                for (x1 = x1_min + 1; x1 <= x1_max; x1++) {
                    data_down = r->raw[y - 1][x + x1];
                    /* find the min among the neighbors
                     * in the last row */
                    m1 = r->m[data_down];
                    if ((m1 < m) || ((m1 == m) && (r->leftright == 1))) {
                        m = m1;
                        r->least[data] = data_down;
                    }
                    m = MIN(m, r->m[data_down]);
                }
            }

            /* set current m */
            r->m[data] = r->en[data] + m;
        }
    }
    return LQR_OK;
}

/* compute (vertical) visibility map up to given depth
 * (it also calls inflate() to add image enlargment information) */
LqrRetVal
lqr_carver_build_vsmap(LqrCarver *r, int depth)
{
    int l;
    int lr_switch_interval = 0;
    LqrDataTok data_tok;

#ifdef __LQR_VERBOSE__
    printf("[ building visibility map ]\n");
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */

#ifdef __LQR_DEBUG__
    assert(depth <= r->w_start + 1);
    assert(depth >= 1);
#endif /* __LQR_DEBUG__ */

    /* default behaviour : compute all possible levels
     * (complete map) */
    if (depth == 0) {
        depth = r->w_start + 1;
    }

    /* here we assume that
     * lqr_carver_set_width(w_start - max_level + 1);
     * has been given */

    /* left-right switch interval */
    if (r->lr_switch_frequency) {
        lr_switch_interval = (depth - r->max_level - 1) / r->lr_switch_frequency + 1;
    }

    /* cycle over levels */
    for (l = r->max_level; l < depth; l++) {
        LQR_CATCH_CANC(r);

        if ((l - r->max_level + r->session_rescale_current) % r->session_update_step == 0) {
            lqr_progress_update(r->progress, (double) (l - r->max_level + r->session_rescale_current) /
                                (double) (r->session_rescale_total));
        }
#ifdef __LQR_DEBUG__
        /* check raw rows */
        lqr_carver_debug_check_rows(r);
#endif /* __LQR_DEBUG__ */

        /* compute vertical seam */
        lqr_carver_build_vpath(r);

        /* update visibility map
         * (assign level to the seam) */
        lqr_carver_update_vsmap(r, l + r->max_level - 1);

        /* increase (in)visibility level
         * (make the last seam invisible) */
        r->level++;
        r->w--;

        /* update raw data */
        lqr_carver_carve(r);

        if (r->w > 1) {
            /* update the energy */
            /* LQR_CATCH (lqr_carver_build_emap (r));  */
            LQR_CATCH(lqr_carver_update_emap(r));

            /* recalculate the minpath map */
            if ((r->lr_switch_frequency) && (((l - r->max_level + lr_switch_interval / 2) % lr_switch_interval) == 0)) {
                r->leftright ^= 1;
                LQR_CATCH(lqr_carver_build_mmap(r));
            } else {
                /* lqr_carver_build_mmap (r); */
                LQR_CATCH(lqr_carver_update_mmap(r));
            }
        } else {
            /* complete the map (last seam) */
            lqr_carver_finish_vsmap(r);
        }
    }

    /* insert seams for image enlargement */
    LQR_CATCH(lqr_carver_inflate(r, depth - 1));

    /* reset image size */
    lqr_carver_set_width(r, r->w_start);
    /* repeat for auxiliary layers */
    data_tok.integer = r->w_start;
    LQR_CATCH(lqr_carver_list_foreach_recursive(r->attached_list, lqr_carver_set_width_attached, data_tok));

#ifdef __LQR_VERBOSE__
    printf("[ visibility map OK ]\n");
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */

    return LQR_OK;
}

/* enlarge the image by seam insertion
 * visibility map is updated and the resulting multisize image
 * is complete in both directions */
LqrRetVal
lqr_carver_inflate(LqrCarver *r, int l)
{
    int w1, z0, vs, k;
    int x, y;
    int c_left;
    void *new_rgb = NULL;
    int *new_vs = NULL;
    double tmp_rgb;
    float *new_bias = NULL;
    float *new_rigmask = NULL;
    LqrDataTok data_tok;
    LqrCarverState prev_state = LQR_CARVER_STATE_STD;

#ifdef __LQR_VERBOSE__
    printf("  [ inflating (active=%i) ]\n", r->active);
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */

#ifdef __LQR_DEBUG__
    assert(l + 1 > r->max_level);       /* otherwise is useless */
#endif /* __LQR_DEBUG__ */

    LQR_CATCH_CANC(r);

    if (r->root == NULL) {
        prev_state = atomic_load(&r->state);
        LQR_CATCH(lqr_carver_set_state(r, LQR_CARVER_STATE_INFLATING, true));
    }

    /* first iterate on attached carvers */
    data_tok.integer = l;
    LQR_CATCH(lqr_carver_list_foreach(r->attached_list, lqr_carver_inflate_attached, data_tok));

    /* scale to current maximum size
     * (this is the original size the first time) */
    lqr_carver_set_width(r, r->w0);

    /* final width */
    w1 = r->w0 + l - r->max_level + 1;

    /* allocate room for new maps */
    BUF_TRY_NEW0_RET_LQR(new_rgb, w1 * r->h0 * r->channels, r->col_depth);

    if (r->root == NULL) {
        LQR_CATCH_MEM(new_vs = LRQ_CALLOC(int, w1 * r->h0));
    }
    if (r->active) {
        if (r->bias) {
            LQR_CATCH_MEM(new_bias = LRQ_CALLOC(float, w1 * r->h0));
        }
        if (r->rigidity_mask) {
            LQR_CATCH_MEM(new_rigmask = LRQ_CALLOC(float, w1 * r->h0));
        }
    }

    /* span the image with a cursor
     * and build the new image */
    lqr_cursor_reset(r->c);
    x = 0;
    y = 0;
    for (z0 = 0; z0 < w1 * r->h0; z0++, lqr_cursor_next(r->c)) {

        LQR_CATCH_CANC(r);

        /* read visibility */
        vs = r->vs[r->c->now];
        if ((vs != 0) && (vs <= l + r->max_level - 1)
            && (vs >= 2 * r->max_level - 1)) {
            /* the point belongs to a previously computed seam
             * and was not inserted during a previous
             * inflate() call : insert another seam */

            /* the new pixel value is equal to the average of its
             * left and right neighbors */

            if (r->c->x > 0) {
                c_left = lqr_cursor_left(r->c);
            } else {
                c_left = r->c->now;
            }

            for (k = 0; k < r->channels; k++) {
                switch (r->col_depth) {
                    case LQR_COLDEPTH_8I:
                        tmp_rgb = (AS_8I(r->rgb)[c_left * r->channels + k] +
                                   AS_8I(r->rgb)[r->c->now * r->channels + k]) / 2;
                        AS_8I(new_rgb)[z0 * r->channels + k] = (lqr_t_8i) (tmp_rgb + 0.499999);
                        break;
                    case LQR_COLDEPTH_16I:
                        tmp_rgb = (AS_16I(r->rgb)[c_left * r->channels + k] +
                                   AS_16I(r->rgb)[r->c->now * r->channels + k]) / 2;
                        AS_16I(new_rgb)[z0 * r->channels + k] = (lqr_t_16i) (tmp_rgb + 0.499999);
                        break;
                    case LQR_COLDEPTH_32F:
                        tmp_rgb = (AS_32F(r->rgb)[c_left * r->channels + k] +
                                   AS_32F(r->rgb)[r->c->now * r->channels + k]) / 2;
                        AS_32F(new_rgb)[z0 * r->channels + k] = (lqr_t_32f) tmp_rgb;
                        break;
                    case LQR_COLDEPTH_64F:
                        tmp_rgb = (AS_64F(r->rgb)[c_left * r->channels + k] +
                                   AS_64F(r->rgb)[r->c->now * r->channels + k]) / 2;
                        AS_64F(new_rgb)[z0 * r->channels + k] = (lqr_t_64f) tmp_rgb;
                        break;
                }
            }
            if (r->active) {
                if (r->bias) {
                    new_bias[z0] = (r->bias[c_left] + r->bias[r->c->now]) / 2;
                }
                if (r->rigidity_mask) {
                    new_rigmask[z0] = (r->rigidity_mask[c_left] + r->rigidity_mask[r->c->now]) / 2;
                }
            }
            /* the first time inflate() is called
             * the new visibility should be -vs + 1 but we shift it
             * so that the final minimum visibiliy will be 1 again
             * and so that vs=0 still means "uninitialized".
             * Subsequent inflations account for that */
            if (r->root == NULL) {
                new_vs[z0] = l - vs + r->max_level;
            }
            z0++;
        }
        for (k = 0; k < r->channels; k++) {
            PXL_COPY(new_rgb, z0 * r->channels + k, r->rgb, r->c->now * r->channels + k, r->col_depth);
        }
        if (r->active) {
            if (r->bias) {
                new_bias[z0] = r->bias[r->c->now];
            }
            if (r->rigidity_mask) {
                new_rigmask[z0] = r->rigidity_mask[r->c->now];
            }
        }
        if (vs != 0) {
            /* visibility has to be shifted up */
            if (r->root == NULL) {
                new_vs[z0] = vs + l - r->max_level + 1;
            }
        } else if (r->raw != NULL) {
#ifdef __LQR_DEBUG__
            assert(y < r->h_start);
            assert(x < r->w_start - l);
#endif /* __LQR_DEBUG__ */
            r->raw[y][x] = z0;
            x++;
            if (x >= r->w_start - l) {
                x = 0;
                y++;
            }
        }
    }

#ifdef __LQR_DEBUG__
    if (r->raw != NULL) {
        assert(x == 0);
        if (w1 != 2 * r->w_start - 1) {
            assert((y == r->h_start)
                   || (printf("y=%i hst=%i w1=%i\n", y, r->h_start, w1)
                       && fflush(stdout) && 0));
        }
    }
#endif /* __LQR_DEBUG__ */

    /* substitute maps */
    if (!r->preserve_in_buffer) {
        LRQ_FREE(r->rgb);
    }
    /* LRQ_FREE (r->vs); */
    LRQ_FREE(r->en);
    LRQ_FREE(r->m);
    LRQ_FREE(r->rcache);
    LRQ_FREE(r->least);
    LRQ_FREE(r->bias);
    LRQ_FREE(r->rigidity_mask);

    r->bias = NULL;
    r->rcache = NULL;
    r->nrg_uptodate = false;

    r->rgb = new_rgb;
    r->preserve_in_buffer = false;

    if (r->root == NULL) {
        LRQ_FREE(r->vs);
        r->vs = new_vs;
        LQR_CATCH(lqr_carver_propagate_vsmap(r));
    } else {
        /* r->vs = NULL; */
    }
    if (r->nrg_active) {
        LQR_CATCH_MEM(r->en = LRQ_CALLOC(float, w1 * r->h0));
    }
    if (r->active) {
        r->bias = new_bias;
        r->rigidity_mask = new_rigmask;
        LQR_CATCH_MEM(r->m = LRQ_CALLOC(float, w1 * r->h0));
        LQR_CATCH_MEM(r->least = LRQ_CALLOC(int, w1 * r->h0));
    }

    /* set new widths & levels (w_start is kept for reference) */
    r->level = l + 1;
    r->max_level = l + 1;
    r->w0 = w1;
    r->w = r->w_start;

    /* reset readout buffer */
    LRQ_FREE(r->rgb_ro_buffer);
    BUF_TRY_NEW0_RET_LQR(r->rgb_ro_buffer, r->w0 * r->channels, r->col_depth);

#ifdef __LQR_VERBOSE__
    printf("  [ inflating OK ]\n");
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */

    if (r->root == NULL) {
        LQR_CATCH(lqr_carver_set_state(r, prev_state, true));
    }

    return LQR_OK;
}

LqrRetVal
lqr_carver_inflate_attached(LqrCarver *r, LqrDataTok data)
{
    return lqr_carver_inflate(r, data.integer);
}

/*** internal functions for maps computations ***/

/* do the carving
 * this actually carves the raw array,
 * which holds the indices to be used
 * in all the other maps */
void
lqr_carver_carve(LqrCarver *r)
{
    int x, y;

#ifdef __LQR_DEBUG__
    assert(r->root == NULL);
#endif /* __LQR_DEBUG__ */

    for (y = 0; y < r->h_start; y++) {
#ifdef __LQR_DEBUG__
        assert(r->vs[r->raw[y][r->vpath_x[y]]] != 0);
        for (x = 0; x < r->vpath_x[y]; x++) {
            assert(r->vs[r->raw[y][x]] == 0);
        }
#endif /* __LQR_DEBUG__ */
        for (x = r->vpath_x[y]; x < r->w; x++) {
            r->raw[y][x] = r->raw[y][x + 1];
#ifdef __LQR_DEBUG__
            assert(r->vs[r->raw[y][x]] == 0);
#endif /* __LQR_DEBUG__ */
        }
    }

    r->nrg_uptodate = false;
}

/* update energy map after seam removal */
LqrRetVal
lqr_carver_update_emap(LqrCarver *r)
{
    int x, y;
    int y1, y1_min, y1_max;

    LQR_CATCH_CANC(r);

    if (r->nrg_uptodate) {
        return LQR_OK;
    }
    if (r->use_rcache) {
        LQR_CATCH_F(r->rcache != NULL);
    }

    for (y = 0; y < r->h; y++) {
        /* note: here the vpath has already
         * been carved */
        x = r->vpath_x[y];
        r->nrg_xmin[y] = x;
        r->nrg_xmax[y] = x - 1;
    }
    for (y = 0; y < r->h; y++) {
        x = r->vpath_x[y];
        y1_min = MAX(y - r->nrg_radius, 0);
        y1_max = MIN(y + r->nrg_radius, r->h - 1);

        for (y1 = y1_min; y1 <= y1_max; y1++) {
            r->nrg_xmin[y1] = MIN(r->nrg_xmin[y1], x - r->nrg_radius);
            r->nrg_xmin[y1] = MAX(0, r->nrg_xmin[y1]);
            /* note: the -1 below is because of the previous carving */
            r->nrg_xmax[y1] = MAX(r->nrg_xmax[y1], x + r->nrg_radius - 1);
            r->nrg_xmax[y1] = MIN(r->w - 1, r->nrg_xmax[y1]);
        }
    }

    for (y = 0; y < r->h; y++) {
        LQR_CATCH_CANC(r);

        for (x = r->nrg_xmin[y]; x <= r->nrg_xmax[y]; x++) {
            LQR_CATCH(lqr_carver_compute_e(r, x, y));
        }
    }

    r->nrg_uptodate = true;

    return LQR_OK;
}

/* update the auxiliary minpath map
 * this only updates the affected pixels,
 * which start form the beginning of the changed
 * energy region around the seam and expand
 * at most by delta_x (in both directions)
 * at each row */
LqrRetVal
lqr_carver_update_mmap(LqrCarver *r)
{
    int x, y;
    int x_min, x_max;
    int x1, dx;
    int x1_min, x1_max;
    int data, data_down, least;
    float m, m1, r_fact;
    float new_m;
    float *mc = NULL;
    int stop;
    int x_stop;

    LQR_CATCH_CANC(r);
    LQR_CATCH_F(r->nrg_uptodate);

    if (r->rigidity) {
        LQR_CATCH_MEM(mc = LRQ_CALLOC(float, 2 * r->delta_x + 1));
        mc += r->delta_x;
    }

    /* span first row */
    /* x_min = MAX (r->vpath_x[0] - r->delta_x, 0); */
    x_min = MAX(r->nrg_xmin[0], 0);
    /* x_max = MIN (r->vpath_x[0] + r->delta_x - 1, r->w - 1); */
    /* x_max = MIN (r->vpath_x[0] + r->delta_x, r->w - 1); */
    x_max = MIN(r->nrg_xmax[0], r->w - 1);

    for (x = x_min; x <= x_max; x++) {
        data = r->raw[0][x];
        r->m[data] = r->en[data];
    }

    /* other rows */
    for (y = 1; y < r->h; y++) {
        LQR_CATCH_CANC(r);

        /* make sure to include the changed energy region */
        x_min = MIN(x_min, r->nrg_xmin[y]);
        x_max = MAX(x_max, r->nrg_xmax[y]);

        /* expand the affected region by delta_x */
        x_min = MAX(x_min - r->delta_x, 0);
        x_max = MIN(x_max + r->delta_x, r->w - 1);

        /* span the affected region */
        stop = 0;
        x_stop = 0;
        for (x = x_min; x <= x_max; x++) {
            data = r->raw[y][x];
            if (r->rigidity_mask) {
                r_fact = r->rigidity_mask[data];
            } else {
                r_fact = 1;
            }

            /* find the minimum in the previous rows
             * as in build_mmap() */
            x1_min = MAX(0, x - r->delta_x);
            x1_max = MIN(r->w - 1, x + r->delta_x);

            if (r->rigidity) {
                dx = x1_min - x;
                switch (x1_max - x1_min + 1) {
                    UPDATE_MMAP_OPTIMISED_CASES_RIG
                    default:
                        data_down = r->raw[y - 1][x1_min];
                        least = data_down;
                        m = r->m[data_down] + r_fact * r->rigidity_map[dx++];
                        /* fprintf(stderr, "y,x=%i,%i x1=%i dx=%i mr=%g MR=%g m=%g M=%g\n", y, x, x1_min, dx, m, MRDOWN(y, x1_min, dx), r->m[data_down], MDOWN(y, x1_min)); fflush(stderr);   */
                        for (x1 = x1_min + 1; x1 <= x1_max; x1++, dx++) {
                            data_down = r->raw[y - 1][x1];
                            m1 = r->m[data_down] + r_fact * r->rigidity_map[dx];
                            /* fprintf(stderr, "y,x=%i,%i x1=%i dx=%i mr=%g MR=%g m=%g M=%g\n", y, x, x1, dx, m1, MRDOWN(y, x1, dx), r->m[data_down], MDOWN(y, x1)); fflush(stderr);   */
                            if ((m1 < m) || ((m1 == m) && (r->leftright == 1))) {
                                m = m1;
                                least = data_down;
                            }
                        }
                }
                /* fprintf(stderr, "y,x=%i,%i x1_min,max=%i,%i least=%i m=%g\n", y, x, x1_min, x1_max, least, m); fflush(stderr); */
            } else {
                switch (x1_max - x1_min + 1) {
                    UPDATE_MMAP_OPTIMISED_CASES
                    default:
                        data_down = r->raw[y - 1][x1_min];
                        least = data_down;
                        m = r->m[data_down];
                        for (x1 = x1_min + 1; x1 <= x1_max; x1++) {
                            data_down = r->raw[y - 1][x1];
                            m1 = r->m[data_down];
                            if ((m1 < m) || ((m1 == m) && (r->leftright == 1))) {
                                m = m1;
                                least = data_down;
                            }
                        }
                }
                /* fprintf(stderr, "y,x=%i,%i x1_min,max=%i,%i least=%i m=%g\n", y, x, x1_min, x1_max, least, m); fflush(stderr);   */
            }

            new_m = r->en[data] + m;

            /* reduce the range if there's no (relevant) difference
             * with the previous map */
            if (r->least[data] == least) {
                if (fabsf(r->m[data] - new_m) < UPDATE_TOLERANCE) {
                    if (stop == 0) {
                        x_stop = x;
                    }
                    stop = 1;
                    new_m = r->m[data];
                } else {
                    stop = 0;
                    r->m[data] = new_m;
                }
                if ((x == x_min) && stop) {
                    x_min++;
                }
            } else {
                stop = 0;
                r->m[data] = new_m;
            }

            r->least[data] = least;

            if ((x == x_max) && (stop)) {
                x_max = x_stop;
            }
        }

    }

    if (r->rigidity) {
        mc -= r->delta_x;
        LRQ_FREE(mc);
    }

    return LQR_OK;
}

/* compute seam path from minpath map */
void
lqr_carver_build_vpath(LqrCarver *r)
{
    int x, y, z0;
    float m, m1;
    int last = -1;
    int last_x = 0;
    int x_min, x_max;

    /* we start at last row */
    y = r->h - 1;

    /* span the last row for the minimum mmap value */
    m = (1 << 29);
    for (x = 0, z0 = y * r->w_start; x < r->w; x++, z0++) {
#ifdef __LQR_DEBUG__
        assert(r->vs[r->raw[y][x]] == 0);
#endif /* __LQR_DEBUG__ */

        m1 = r->m[r->raw[y][x]];
        if ((m1 < m) || ((m1 == m) && (r->leftright == 1))) {
            last = r->raw[y][x];
            last_x = x;
            m = m1;
        }
    }

#ifdef __LQR_DEBUG__
    assert(last >= 0);
#endif /* __LQR_DEBUG__ */

    /* follow the track for the other rows */
    for (y = r->h0 - 1; y >= 0; y--) {
#ifdef __LQR_DEBUG__
        assert(r->vs[last] == 0);
        assert(last_x < r->w);
#endif /* __LQR_DEBUG__ */
        r->vpath[y] = last;
        r->vpath_x[y] = last_x;
        if (y > 0) {
            last = r->least[r->raw[y][last_x]];
            /* we also need to retrieve the x coordinate */
            x_min = MAX(last_x - r->delta_x, 0);
            x_max = MIN(last_x + r->delta_x, r->w - 1);
            for (x = x_min; x <= x_max; x++) {
                if (r->raw[y - 1][x] == last) {
                    last_x = x;
                    break;
                }
            }
#ifdef __LQR_DEBUG__
            assert(x < x_max + 1);
#endif /* __LQR_DEBUG__ */
        }
    }

#if 0
    /* we backtrack the seam following the min mmap */
    for (y = r->h0 - 1; y >= 0; y--) {
#ifdef __LQR_DEBUG__
        assert(r->vs[last] == 0);
        assert(last_x < r->w);
#endif /* __LQR_DEBUG__ */

        r->vpath[y] = last;
        r->vpath_x[y] = last_x;
        if (y > 0) {
            m = (1 << 29);
            x_min = MAX(0, last_x - r->delta_x);
            x_max = MIN(r->w - 1, last_x + r->delta_x);
            for (x = x_min; x <= x_max; x++) {
                m1 = r->m[r->raw[y - 1][x]];
                if (m1 < m) {
                    last = r->raw[y - 1][x];
                    last_x = x;
                    m = m1;
                }
            }
        }
    }
#endif
}

/* update visibility map after seam computation */
void
lqr_carver_update_vsmap(LqrCarver *r, int l)
{
    int y;
#ifdef __LQR_DEBUG__
    assert(r->root == NULL);
#endif /* __LQR_DEBUG__ */
    for (y = 0; y < r->h; y++) {
#ifdef __LQR_DEBUG__
        assert(r->vs[r->vpath[y]] == 0);
        assert(r->vpath[y] == r->raw[y][r->vpath_x[y]]);
#endif /* __LQR_DEBUG__ */
        r->vs[r->vpath[y]] = l;
    }
}

/* complete visibility map (last seam) */
/* set the last column of pixels to vis. level w0 */
void
lqr_carver_finish_vsmap(LqrCarver *r)
{
    int y;

#ifdef __LQR_DEBUG__
    assert(r->w == 1);
    assert(r->root == NULL);
#endif /* __LQR_DEBUG__ */
    lqr_cursor_reset(r->c);
    for (y = 1; y <= r->h; y++, lqr_cursor_next(r->c)) {
#ifdef __LQR_DEBUG__
        assert(r->vs[r->c->now] == 0);
#endif /* __LQR_DEBUG__ */
        r->vs[r->c->now] = r->w0;
    }
    lqr_cursor_reset(r->c);
}

/* propagate the root carver's visibility map */
LqrRetVal
lqr_carver_propagate_vsmap(LqrCarver *r)
{
    LqrDataTok data_tok;

    LQR_CATCH_CANC(r);

    data_tok.data = NULL;
    LQR_CATCH(lqr_carver_list_foreach_recursive(r->attached_list, lqr_carver_propagate_vsmap_attached, data_tok));
    return LQR_OK;
}

LqrRetVal
lqr_carver_propagate_vsmap_attached(LqrCarver *r, LqrDataTok data)
{
    /* LqrDataTok data_tok;
    data_tok.data = NULL; */
    r->vs = r->root->vs;
    lqr_carver_scan_reset(r);
    /* LQR_CATCH (lqr_carver_list_foreach (r->attached_list,  lqr_carver_propagate_vsmap_attached, data_tok)); */
    return LQR_OK;
}

/*** image manipulations ***/

/* set width of the multisize image
 * (maps have to be computed already) */
void
lqr_carver_set_width(LqrCarver *r, int w1)
{
#ifdef __LQR_DEBUG__
    assert(w1 <= r->w0);
    assert(w1 >= r->w_start - r->max_level + 1);
#endif /* __LQR_DEBUG__ */
    r->w = w1;
    r->level = r->w0 - w1 + 1;
}

LqrRetVal
lqr_carver_set_width_attached(LqrCarver *r, LqrDataTok data)
{
    lqr_carver_set_width(r, data.integer);
    return LQR_OK;
}

/* flatten the image to its current state
 * (all maps are reset, invisible points are lost) */
/* LQR_PUBLIC */
LqrRetVal
lqr_carver_flatten(LqrCarver *r)
{
    void *new_rgb = NULL;
    float *new_bias = NULL;
    float *new_rigmask = NULL;
    int x, y, k;
    int z0;
    LqrDataTok data_tok;
    LqrCarverState prev_state = LQR_CARVER_STATE_STD;

#ifdef __LQR_VERBOSE__
    printf("    [ flattening (active=%i) ]\n", r->active);
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */

    LQR_CATCH_CANC(r);

    if (r->root == NULL) {
        prev_state = atomic_load(&r->state);
        LQR_CATCH(lqr_carver_set_state(r, LQR_CARVER_STATE_FLATTENING, true));
    }

    /* first iterate on attached carvers */
    data_tok.data = NULL;
    LQR_CATCH(lqr_carver_list_foreach(r->attached_list, lqr_carver_flatten_attached, data_tok));

    /* free non needed maps first */
    LRQ_FREE(r->en);
    LRQ_FREE(r->m);
    LRQ_FREE(r->rcache);
    LRQ_FREE(r->least);

    r->rcache = NULL;
    r->nrg_uptodate = false;

    /* allocate room for new map */
    BUF_TRY_NEW0_RET_LQR(new_rgb, r->w * r->h * r->channels, r->col_depth);

    if (r->active) {
        if (r->rigidity_mask) {
            LQR_CATCH_MEM(new_rigmask = LRQ_CALLOC(float, r->w * r->h));
        }
    }
    if (r->nrg_active) {
        if (r->bias) {
            LQR_CATCH_MEM(new_bias = LRQ_CALLOC(float, r->w * r->h));
        }
        LRQ_FREE(r->_raw);
        LRQ_FREE(r->raw);
        LQR_CATCH_MEM(r->_raw = LRQ_CALLOC(int, r->w * r->h));
        LQR_CATCH_MEM(r->raw = LRQ_CALLOC(int *, r->h));
    }

    /* span the image with the cursor and copy
     * it in the new array  */
    lqr_cursor_reset(r->c);
    for (y = 0; y < r->h; y++) {
        LQR_CATCH_CANC(r);

        if (r->nrg_active) {
            r->raw[y] = r->_raw + y * r->w;
        }
        for (x = 0; x < r->w; x++) {
            z0 = y * r->w + x;
            for (k = 0; k < r->channels; k++) {
                PXL_COPY(new_rgb, z0 * r->channels + k, r->rgb, r->c->now * r->channels + k, r->col_depth);
            }
            if (r->active) {
                if (r->rigidity_mask) {
                    new_rigmask[z0] = r->rigidity_mask[r->c->now];
                }
            }
            if (r->nrg_active) {
                if (r->bias) {
                    new_bias[z0] = r->bias[r->c->now];
                }
                r->raw[y][x] = z0;
            }
            lqr_cursor_next(r->c);
        }
    }

    /* substitute the old maps */
    if (!r->preserve_in_buffer) {
        LRQ_FREE(r->rgb);
    }
    r->rgb = new_rgb;
    r->preserve_in_buffer = false;
    if (r->nrg_active) {
        LRQ_FREE(r->bias);
        r->bias = new_bias;
    }
    if (r->active) {
        LRQ_FREE(r->rigidity_mask);
        r->rigidity_mask = new_rigmask;
    }

    /* init the other maps */
    if (r->root == NULL) {
        LRQ_FREE(r->vs);
        LQR_CATCH_MEM(r->vs = LRQ_CALLOC(int, r->w * r->h));
        LQR_CATCH(lqr_carver_propagate_vsmap(r));
    }
    if (r->nrg_active) {
        LQR_CATCH_MEM(r->en = LRQ_CALLOC(float, r->w * r->h));
    }
    if (r->active) {
        LQR_CATCH_MEM(r->m = LRQ_CALLOC(float, r->w * r->h));
        LQR_CATCH_MEM(r->least = LRQ_CALLOC(int, r->w * r->h));
    }

    /* reset widths, heights & levels */
    r->w0 = r->w;
    r->h0 = r->h;
    r->w_start = r->w;
    r->h_start = r->h;
    r->level = 1;
    r->max_level = 1;

#ifdef __LQR_VERBOSE__
    printf("    [ flattening OK ]\n");
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */

    if (r->root == NULL) {
        LQR_CATCH(lqr_carver_set_state(r, prev_state, true));
    }

    return LQR_OK;
}

LqrRetVal
lqr_carver_flatten_attached(LqrCarver *r, LqrDataTok data)
{
    return lqr_carver_flatten(r);
}

/* transpose the image, in its current state
 * (all maps and invisible points are lost) */
LqrRetVal
lqr_carver_transpose(LqrCarver *r)
{
    int x, y, k;
    int z0, z1;
    int d;
    void *new_rgb = NULL;
    float *new_bias = NULL;
    float *new_rigmask = NULL;
    LqrDataTok data_tok;
    LqrCarverState prev_state = LQR_CARVER_STATE_STD;

#ifdef __LQR_VERBOSE__
    printf("[ transposing (active=%i) ]\n", r->active);
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */

    LQR_CATCH_CANC(r);

    if (r->root == NULL) {
        prev_state = atomic_load(&r->state);
        LQR_CATCH(lqr_carver_set_state(r, LQR_CARVER_STATE_TRANSPOSING, true));
    }

    if (r->level > 1) {
        LQR_CATCH(lqr_carver_flatten(r));
    }

    /* first iterate on attached carvers */
    data_tok.data = NULL;
    LQR_CATCH(lqr_carver_list_foreach(r->attached_list, lqr_carver_transpose_attached, data_tok));

    /* free non needed maps first */
    if (r->root == NULL) {
        LRQ_FREE(r->vs);
    }
    LRQ_FREE(r->en);
    LRQ_FREE(r->m);
    LRQ_FREE(r->rcache);
    LRQ_FREE(r->least);
    LRQ_FREE(r->rgb_ro_buffer);

    r->rcache = NULL;
    r->nrg_uptodate = false;

    /* allocate room for the new maps */
    BUF_TRY_NEW0_RET_LQR(new_rgb, r->w0 * r->h0 * r->channels, r->col_depth);

    if (r->active) {
        if (r->rigidity_mask) {
            LQR_CATCH_MEM(new_rigmask = LRQ_CALLOC(float, r->w0 * r->h0));
        }
    }
    if (r->nrg_active) {
        if (r->bias) {
            LQR_CATCH_MEM(new_bias = LRQ_CALLOC(float, r->w0 * r->h0));
        }
        LRQ_FREE(r->_raw);
        LRQ_FREE(r->raw);
        LQR_CATCH_MEM(r->_raw = LRQ_CALLOC(int, r->h0 * r->w0));
        LQR_CATCH_MEM(r->raw = LRQ_CALLOC(int *, r->w0));
    }

    /* compute trasposed maps */
    for (x = 0; x < r->w; x++) {
        if (r->nrg_active) {
            r->raw[x] = r->_raw + x * r->h0;
        }
        for (y = 0; y < r->h; y++) {
            z0 = y * r->w0 + x;
            z1 = x * r->h0 + y;
            for (k = 0; k < r->channels; k++) {
                PXL_COPY(new_rgb, z1 * r->channels + k, r->rgb, z0 * r->channels + k, r->col_depth);
            }
            if (r->active) {
                if (r->rigidity_mask) {
                    new_rigmask[z1] = r->rigidity_mask[z0];
                }
            }
            if (r->nrg_active) {
                if (r->bias) {
                    new_bias[z1] = r->bias[z0];
                }
                r->raw[x][y] = z1;
            }
        }
    }

    /* substitute the map */
    if (!r->preserve_in_buffer) {
        LRQ_FREE(r->rgb);
    }
    r->rgb = new_rgb;
    r->preserve_in_buffer = false;

    if (r->nrg_active) {
        LRQ_FREE(r->bias);
        r->bias = new_bias;
    }
    if (r->active) {
        LRQ_FREE(r->rigidity_mask);
        r->rigidity_mask = new_rigmask;
    }

    /* init the other maps */
    if (r->root == NULL) {
        LQR_CATCH_MEM(r->vs = LRQ_CALLOC(int, r->w0 * r->h0));
        LQR_CATCH(lqr_carver_propagate_vsmap(r));
    }
    if (r->nrg_active) {
        LQR_CATCH_MEM(r->en = LRQ_CALLOC(float, r->w0 * r->h0));
    }
    if (r->active) {
        LQR_CATCH_MEM(r->m = LRQ_CALLOC(float, r->w0 * r->h0));
        LQR_CATCH_MEM(r->least = LRQ_CALLOC(int, r->w0 * r->h0));
    }

    /* switch widths & heights */
    d = r->w0;
    r->w0 = r->h0;
    r->h0 = d;
    r->w = r->w0;
    r->h = r->h0;

    /* reset w_start, h_start & levels */
    r->w_start = r->w0;
    r->h_start = r->h0;
    r->level = 1;
    r->max_level = 1;

    /* reset seam path, cursor and readout buffer */
    if (r->active) {
        LRQ_FREE(r->vpath);
        LQR_CATCH_MEM(r->vpath = LRQ_CALLOC(int, r->h));
        LRQ_FREE(r->vpath_x);
        LQR_CATCH_MEM(r->vpath_x = LRQ_CALLOC(int, r->h));
        LRQ_FREE(r->nrg_xmin);
        LQR_CATCH_MEM(r->nrg_xmin = LRQ_CALLOC(int, r->h));
        LRQ_FREE(r->nrg_xmax);
        LQR_CATCH_MEM(r->nrg_xmax = LRQ_CALLOC(int, r->h));
    }

    BUF_TRY_NEW0_RET_LQR(r->rgb_ro_buffer, r->w0 * r->channels, r->col_depth);

    /* rescale rigidity */

    if (r->active) {
        for (x = -r->delta_x; x <= r->delta_x; x++) {
            r->rigidity_map[x] = r->rigidity_map[x] * r->w0 / r->h0;
        }
    }

    /* set transposed flag */
    r->transposed = (r->transposed ? 0 : 1);

#ifdef __LQR_VERBOSE__
    printf("[ transpose OK ]\n");
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */

    if (r->root == NULL) {
        LQR_CATCH(lqr_carver_set_state(r, prev_state, true));
    }

    return LQR_OK;
}

LqrRetVal
lqr_carver_transpose_attached(LqrCarver *r, LqrDataTok data)
{
    return lqr_carver_transpose(r);
}

/* resize w + h: these are the liquid rescale methods.
 * They automatically determine the depth of the map
 * according to the desired size, can be called multiple
 * times, transpose the image as necessasry */
LqrRetVal
lqr_carver_resize_width(LqrCarver *r, int w1)
{
    LqrDataTok data_tok;
    int delta, gamma;
    int delta_max;
    /* delta is used to determine the required depth
     * gamma to decide if action is necessary */
    if (!r->transposed) {
        delta = w1 - r->w_start;
        gamma = w1 - r->w;
        delta_max = (int) ((r->enl_step - 1) * r->w_start) - 1;
    } else {
        delta = w1 - r->h_start;
        gamma = w1 - r->h;
        delta_max = (int) ((r->enl_step - 1) * r->h_start) - 1;
    }
    if (delta_max < 1) {
        delta_max = 1;
    }
    if (delta < 0) {
        delta = -delta;
        delta_max = delta;
    }

    LQR_CATCH_CANC(r);
    LQR_CATCH_F(atomic_load(&r->state) == LQR_CARVER_STATE_STD);
    LQR_CATCH(lqr_carver_set_state(r, LQR_CARVER_STATE_RESIZING, true));

    /* update step for progress reprt */
    r->session_rescale_total = gamma > 0 ? gamma : -gamma;
    r->session_rescale_current = 0;
    r->session_update_step = (int) MAX(r->session_rescale_total * r->progress->update_step, 1);

    if (r->session_rescale_total) {
        lqr_progress_init(r->progress, r->progress->init_width_message);
    }

    while (gamma) {
        int delta0 = MIN(delta, delta_max);
        int new_w;

        delta -= delta0;
        if (r->transposed) {
            LQR_CATCH(lqr_carver_transpose(r));
        }
        new_w = MIN(w1, r->w_start + delta_max);
        gamma = w1 - new_w;
        LQR_CATCH(lqr_carver_build_maps(r, delta0 + 1));
        lqr_carver_set_width(r, new_w);

        data_tok.integer = new_w;
        lqr_carver_list_foreach_recursive(r->attached_list, lqr_carver_set_width_attached, data_tok);

        r->session_rescale_current = r->session_rescale_total - (gamma > 0 ? gamma : -gamma);

        if (r->dump_vmaps) {
            LQR_CATCH(lqr_vmap_internal_dump(r));
        }
        if (new_w < w1) {
            LQR_CATCH(lqr_carver_flatten(r));
            delta_max = (int) ((r->enl_step - 1) * r->w_start) - 1;
            if (delta_max < 1) {
                delta_max = 1;
            }
        }
    }

    if (r->session_rescale_total) {
        lqr_progress_end(r->progress, r->progress->end_width_message);
    }

    LQR_CATCH(lqr_carver_set_state(r, LQR_CARVER_STATE_STD, true));

    return LQR_OK;
}

LqrRetVal
lqr_carver_resize_height(LqrCarver *r, int h1)
{
    LqrDataTok data_tok;
    int delta, gamma;
    int delta_max;
    /* delta is used to determine the required depth
     * gamma to decide if action is necessary */
    if (!r->transposed) {
        delta = h1 - r->h_start;
        gamma = h1 - r->h;
        delta_max = (int) ((r->enl_step - 1) * r->h_start) - 1;
    } else {
        delta = h1 - r->w_start;
        gamma = h1 - r->w;
        delta_max = (int) ((r->enl_step - 1) * r->w_start) - 1;
    }
    if (delta_max < 1) {
        delta_max = 1;
    }
    if (delta < 0) {
        delta_max = -delta;
    }
    delta = delta > 0 ? delta : -delta;

    LQR_CATCH_CANC(r);
    LQR_CATCH_F(atomic_load(&r->state) == LQR_CARVER_STATE_STD);
    LQR_CATCH(lqr_carver_set_state(r, LQR_CARVER_STATE_RESIZING, true));

    /* update step for progress reprt */
    r->session_rescale_total = gamma > 0 ? gamma : -gamma;
    r->session_rescale_current = 0;
    r->session_update_step = (int) MAX(r->session_rescale_total * r->progress->update_step, 1);

    if (r->session_rescale_total) {
        lqr_progress_init(r->progress, r->progress->init_height_message);
    }

    while (gamma) {
        int delta0 = MIN(delta, delta_max);
        int new_w;
        delta -= delta0;
        if (!r->transposed) {
            LQR_CATCH(lqr_carver_transpose(r));
        }
        new_w = MIN(h1, r->w_start + delta_max);
        gamma = h1 - new_w;
        LQR_CATCH(lqr_carver_build_maps(r, delta0 + 1));
        lqr_carver_set_width(r, new_w);

        data_tok.integer = new_w;
        lqr_carver_list_foreach_recursive(r->attached_list, lqr_carver_set_width_attached, data_tok);

        r->session_rescale_current = r->session_rescale_total - (gamma > 0 ? gamma : -gamma);

        if (r->dump_vmaps) {
            LQR_CATCH(lqr_vmap_internal_dump(r));
        }
        if (new_w < h1) {
            LQR_CATCH(lqr_carver_flatten(r));
            delta_max = (int) ((r->enl_step - 1) * r->w_start) - 1;
            if (delta_max < 1) {
                delta_max = 1;
            }
        }
    }

    if (r->session_rescale_total) {
        lqr_progress_end(r->progress, r->progress->end_height_message);
    }

    LQR_CATCH(lqr_carver_set_state(r, LQR_CARVER_STATE_STD, true));

    return LQR_OK;
}

/* liquid rescale public method */
/* LQR_PUBLIC */
LqrRetVal
lqr_carver_resize(LqrCarver *r, int w1, int h1)
{
#ifdef __LQR_VERBOSE__
    printf("[ Rescale from %i,%i to %i,%i ]\n", (r->transposed ? r->h : r->w), (r->transposed ? r->w : r->h), w1, h1);
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */
    LQR_CATCH_F((w1 >= 1) && (h1 >= 1));
    LQR_CATCH_F(r->root == NULL);

    LQR_CATCH_CANC(r);
    LQR_CATCH_F(atomic_load(&r->state) == LQR_CARVER_STATE_STD);

    switch (r->resize_order) {
        case LQR_RES_ORDER_HOR:
            LQR_CATCH(lqr_carver_resize_width(r, w1));
            LQR_CATCH(lqr_carver_resize_height(r, h1));
            break;
        case LQR_RES_ORDER_VERT:
            LQR_CATCH(lqr_carver_resize_height(r, h1));
            LQR_CATCH(lqr_carver_resize_width(r, w1));
            break;
#ifdef __LQR_DEBUG__
        default:
            assert(0);
#endif /* __LQR_DEBUG__ */
    }
    lqr_carver_scan_reset_all(r);

#ifdef __LQR_VERBOSE__
    printf("[ Rescale OK ]\n");
    fflush(stdout);
#endif /* __LQR_VERBOSE__ */
    return LQR_OK;
}

LqrRetVal
lqr_carver_set_state(LqrCarver *r, LqrCarverState state, bool skip_canceled)
{
    LqrDataTok data_tok;
    int lock_pos;

    LQR_CATCH_F(r->root == NULL);

    lock_pos = atomic_fetch_add(&r->state_lock_queue, 1);

    while (atomic_load(&r->state_lock) != lock_pos) {
        sleep(10);
    }

    if (skip_canceled && atomic_load(&r->state) == LQR_CARVER_STATE_CANCELLED) {
        atomic_fetch_add(&r->state_lock, 1);
        return LQR_OK;
    }

    atomic_store(&r->state, state);

    data_tok.integer = state;
    LQR_CATCH(lqr_carver_list_foreach_recursive(r->attached_list, lqr_carver_set_state_attached, data_tok));

    atomic_fetch_add(&r->state_lock, 1);

    return LQR_OK;
}

LqrRetVal
lqr_carver_set_state_attached(LqrCarver *r, LqrDataTok data)
{
    atomic_store(&r->state, data.integer);
    return LQR_OK;
}

/* cancel the current action from a different thread */
/* LQR_PUBLIC */
LqrRetVal
lqr_carver_cancel(LqrCarver *r)
{
    LqrCarverState curr_state;

    LQR_CATCH_F(r->root == NULL);

    curr_state = atomic_load(&r->state);

    if ((curr_state == LQR_CARVER_STATE_RESIZING) ||
        (curr_state == LQR_CARVER_STATE_INFLATING) ||
        (curr_state == LQR_CARVER_STATE_TRANSPOSING) || (curr_state == LQR_CARVER_STATE_FLATTENING)) {
        LQR_CATCH(lqr_carver_set_state(r, LQR_CARVER_STATE_CANCELLED, true));
    }
    return LQR_OK;
}

/* get current size */
/* LQR_PUBLIC */
int
lqr_carver_get_width(LqrCarver *r)
{
    return (r->transposed ? r->h : r->w);
}

/* LQR_PUBLIC */
int
lqr_carver_get_height(LqrCarver *r)
{
    return (r->transposed ? r->w : r->h);
}

/* get reference size */
/* LQR_PUBLIC */
int
lqr_carver_get_ref_width(LqrCarver *r)
{
    return (r->transposed ? r->h_start : r->w_start);
}

/* LQR_PUBLIC */
int
lqr_carver_get_ref_height(LqrCarver *r)
{
    return (r->transposed ? r->w_start : r->h_start);
}

/* get colour channels */
/* LQR_PUBLIC */
int
lqr_carver_get_channels(LqrCarver *r)
{
    return r->channels;
}

/* LQR_PUBLIC */
int
lqr_carver_get_bpp(LqrCarver *r)
{
    return lqr_carver_get_channels(r);
}

/* get colour depth */
/* LQR_PUBLIC */
LqrColDepth
lqr_carver_get_col_depth(LqrCarver *r)
{
    return r->col_depth;
}

/* get image type */
/* LQR_PUBLIC */
LqrImageType
lqr_carver_get_image_type(LqrCarver *r)
{
    return r->image_type;
}

/* get enlargement step */
/* LQR_PUBLIC */
float
lqr_carver_get_enl_step(LqrCarver *r)
{
    return r->enl_step;
}

/* get orientation */
/* LQR_PUBLIC */
int
lqr_carver_get_orientation(LqrCarver *r)
{
    return (r->transposed ? 1 : 0);
}

/* get depth */
/* LQR_PUBLIC */
int
lqr_carver_get_depth(LqrCarver *r)
{
    return r->w0 - r->w_start;
}

/* readout reset */
/* LQR_PUBLIC */
void
lqr_carver_scan_reset(LqrCarver *r)
{
    lqr_cursor_reset(r->c);
}

LqrRetVal
lqr_carver_scan_reset_attached(LqrCarver *r, LqrDataTok data)
{
    lqr_carver_scan_reset(r);
    return lqr_carver_list_foreach(r->attached_list, lqr_carver_scan_reset_attached, data);
}

void
lqr_carver_scan_reset_all(LqrCarver *r)
{
    LqrDataTok data;
    data.data = NULL;
    lqr_carver_scan_reset(r);
    lqr_carver_list_foreach(r->attached_list, lqr_carver_scan_reset_attached, data);
}

/* readout all, pixel by bixel */
/* LQR_PUBLIC */
bool
lqr_carver_scan(LqrCarver *r, int *x, int *y, uint8_t **rgb)
{
    int k;
    if (r->col_depth != LQR_COLDEPTH_8I) {
        return false;
    }
    if (r->c->eoc) {
        lqr_carver_scan_reset(r);
        return false;
    }
    (*x) = (r->transposed ? r->c->y : r->c->x);
    (*y) = (r->transposed ? r->c->x : r->c->y);
    for (k = 0; k < r->channels; k++) {
        AS_8I(r->rgb_ro_buffer)[k] = AS_8I(r->rgb)[r->c->now * r->channels + k];
    }
    (*rgb) = AS_8I(r->rgb_ro_buffer);
    lqr_cursor_next(r->c);
    return true;
}

/* LQR_PUBLIC */
bool
lqr_carver_scan_ext(LqrCarver *r, int *x, int *y, void **rgb)
{
    int k;
    if (r->c->eoc) {
        lqr_carver_scan_reset(r);
        return false;
    }
    (*x) = (r->transposed ? r->c->y : r->c->x);
    (*y) = (r->transposed ? r->c->x : r->c->y);
    for (k = 0; k < r->channels; k++) {
        PXL_COPY(r->rgb_ro_buffer, k, r->rgb, r->c->now * r->channels + k, r->col_depth);
    }

    BUF_POINTER_COPY(rgb, r->rgb_ro_buffer, r->col_depth);

    lqr_cursor_next(r->c);
    return true;
}

/* readout all, by line */
/* LQR_PUBLIC */
bool
lqr_carver_scan_by_row(LqrCarver *r)
{
    return r->transposed ? false : true;
}

/* LQR_PUBLIC */
bool
lqr_carver_scan_line(LqrCarver *r, int *n, uint8_t **rgb)
{
    if (r->col_depth != LQR_COLDEPTH_8I) {
        return false;
    }
    return lqr_carver_scan_line_ext(r, n, (void **) rgb);
}

/* LQR_PUBLIC */
bool
lqr_carver_scan_line_ext(LqrCarver *r, int *n, void **rgb)
{
    int k, x;
    if (r->c->eoc) {
        lqr_carver_scan_reset(r);
        return false;
    }
    x = r->c->x;
    (*n) = r->c->y;
    while (x > 0) {
        lqr_cursor_prev(r->c);
        x = r->c->x;
    }
    for (x = 0; x < r->w; x++) {
        for (k = 0; k < r->channels; k++) {
            PXL_COPY(r->rgb_ro_buffer, x * r->channels + k, r->rgb, r->c->now * r->channels + k, r->col_depth);
        }
        lqr_cursor_next(r->c);
    }

    BUF_POINTER_COPY(rgb, r->rgb_ro_buffer, r->col_depth);

    return true;
}

#ifdef __LQR_DEBUG__
void
lqr_carver_debug_check_rows(LqrCarver *r)
{
    int x, y;
    int data;
    for (y = 0; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            data = r->raw[y][x];
            if (data / r->w0 != y) {
                fflush(stderr);
            }
            assert(data / r->w0 == y);
        }
    }
}
#endif /* __LQR_DEBUG__ */

/**** END OF LQR_CARVER CLASS FUNCTIONS ****/
