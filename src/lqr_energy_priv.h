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

#ifndef __LQR_ENERGY_PRIV_H__
#define __LQR_ENERGY_PRIV_H__

#ifndef __LQR_BASE_H__
#error "lqr_base.h must be included prior to lqr_energy_priv.h"
#endif /* __LQR_BASE_H__ */

#ifndef __LQR_GRADIENT_H__
#error "lqr_gradient.h must be included prior to lqr_energy_priv.h"
#endif /* __LQR_GRADIENT_H__ */

#ifndef __LQR_READER_WINDOW_PUB_H__
#error "lqr_rwindow_pub.h must be included prior to lqr_energy_priv.h"
#endif /* __LQR_READER_WINDOW_PUB_H__ */

#define LQR_SATURATE_(x) (1 / (1 + (1 / (x))))
#define LQR_SATURATE(x) ((x) >= 0 ? LQR_SATURATE_(x) : -LQR_SATURATE_(-x))

double lqr_pixel_get_norm(void *src, int src_ind, LqrColDepth col_depth);
void lqr_pixel_set_norm(double val, void *rgb, int rgb_ind, LqrColDepth col_depth);
double lqr_pixel_get_rgbcol(void *rgb, int rgb_ind, LqrColDepth col_depth, LqrImageType image_type,
                             int channel);
double lqr_carver_read_brightness_grey(LqrCarver *r, int x, int y);
double lqr_carver_read_brightness_std(LqrCarver *r, int x, int y);
double lqr_carver_read_brightness_custom(LqrCarver *r, int x, int y);
double lqr_carver_read_brightness(LqrCarver *r, int x, int y);
double lqr_carver_read_luma_std(LqrCarver *r, int x, int y);
double lqr_carver_read_luma(LqrCarver *r, int x, int y);
double lqr_carver_read_rgba(LqrCarver *r, int x, int y, int channel);
double lqr_carver_read_custom(LqrCarver *r, int x, int y, int channel);

double lqr_carver_read_cached_std(LqrCarver *r, int x, int y);
double lqr_carver_read_cached_rgba(LqrCarver *r, int x, int y, int channel);
double lqr_carver_read_cached_custom(LqrCarver *r, int x, int y, int channel);

/* cache brightness (or luma or else) to speedup energy computation */
double *lqr_carver_generate_rcache_bright();
double *lqr_carver_generate_rcache_luma();
double *lqr_carver_generate_rcache_rgba();
double *lqr_carver_generate_rcache_custom();
double *lqr_carver_generate_rcache();

float lqr_energy_builtin_grad_all(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow,
                                   LqrGradFunc gf);
float lqr_energy_builtin_grad_norm(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow,
                                    void * extra_data);
float lqr_energy_builtin_grad_sumabs(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow,
                                      void * extra_data);
float lqr_energy_builtin_grad_xabs(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow,
                                    void * extra_data);
float lqr_energy_builtin_null(int x, int y, int img_width, int img_height, LqrReadingWindow *rwindow,
                               void * extra_data);

#endif /* __LQR_ENERGY_PRIV_H__ */
