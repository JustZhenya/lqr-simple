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

#ifndef __LQR_CARVER_BIAS_PUB_H__
#define __LQR_CARVER_BIAS_PUB_H__

#include <stdint.h>

#ifndef __LQR_BASE_H__
#error "lqr_base.h must be included prior to lqr_carver_bias_pub.h"
#endif /* __LQR_BASE_H__ */

/* PUBLIC BIAS-RELATED FUNCTIONS */

LQR_PUBLIC void lqr_carver_bias_clear(LqrCarver *r);
LQR_PUBLIC LqrRetVal lqr_carver_bias_add_xy(LqrCarver *r, double bias, int x, int y);
LQR_PUBLIC LqrRetVal lqr_carver_bias_add_rgb_area(LqrCarver *r, uint8_t *buffer, int bias_factor, int channels,
                                                  int width, int height, int x_off, int y_off);
LQR_PUBLIC LqrRetVal lqr_carver_bias_add_rgb(LqrCarver *r, uint8_t *buffer, int bias_factor, int channels);
LQR_PUBLIC LqrRetVal lqr_carver_bias_add_area(LqrCarver *r, double *buffer, int bias_factor, int width, int height,
                                              int x_off, int y_off);
LQR_PUBLIC LqrRetVal lqr_carver_bias_add(LqrCarver *r, double *buffer, int bias_factor);

#endif /* __LQR_CARVER_BIAS_PUB_H__ */
