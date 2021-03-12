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

#ifndef __LQR_READER_WINDOW_PRIV_H__
#define __LQR_READER_WINDOW_PRIV_H__

#include <stdbool.h>

#ifndef __LQR_BASE_H__
#error "lqr_base.h must be included prior to lqr_rwindow_priv.h"
#endif /* __LQR_BASE_H__ */

struct _LqrReadingWindow {
    double **buffer;
    int radius;
    LqrEnergyReaderType read_t;
    int channels;
    bool use_rcache;
    LqrCarver *carver;
    int x;
    int y;
};

typedef double (*LqrReadFunc) (LqrCarver *, int, int);
typedef double (*LqrReadFuncWithCh) (LqrCarver *, int, int, int);
/* typedef glfoat (*LqrReadFuncAbs) (LqrCarver*, int, int, int, int); */

LqrRetVal lqr_rwindow_fill_std(LqrReadingWindow *rwindow, LqrCarver *r, int x, int y);
LqrRetVal lqr_rwindow_fill_rgba(LqrReadingWindow *rwindow, LqrCarver *r, int x, int y);
LqrRetVal lqr_rwindow_fill_custom(LqrReadingWindow *rwindow, LqrCarver *r, int x, int y);
LqrRetVal lqr_rwindow_fill(LqrReadingWindow *rwindow, LqrCarver *r, int x, int y);

double lqr_rwindow_read_bright(LqrReadingWindow *rwindow, int x, int y);
double lqr_rwindow_read_luma(LqrReadingWindow *rwindow, int x, int y);
double lqr_rwindow_read_rgba(LqrReadingWindow *rwindow, int x, int y, int channel);
double lqr_rwindow_read_custom(LqrReadingWindow *rwindow, int x, int y, int channel);

LqrReadingWindow *lqr_rwindow_new_std(int radius, LqrEnergyReaderType read_func_type, bool use_rcache);
LqrReadingWindow *lqr_rwindow_new_rgba(int radius, bool use_rcache);
LqrReadingWindow *lqr_rwindow_new_custom(int radius, bool use_rcache, int channels);
LqrReadingWindow *lqr_rwindow_new(int radius, LqrEnergyReaderType read_func_type, bool use_rcache);
void lqr_rwindow_destroy(LqrReadingWindow *rwindow);

#endif /* __LQR_READER_WINDOW_PRIV_H__ */
