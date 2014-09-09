/* Copyright 2014. The Regents of the University of California.
 * All rights reserved. Use of this source code is governed by 
 * a BSD-style license which can be found in the LICENSE file.
 *
 * Authors:
 * 2013 Martin Uecker <uecker@eecs.berkeley.edu>
 */


#include <complex.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>

#include "num/multind.h"
#include "num/flpmath.h"
#include "num/fft.h"
#include "num/la.h"

#include "misc/mri.h"
#include "misc/misc.h"
#include "misc/debug.h"

#include "calib/calmat.h"

#include "walsh.h"


	

void walsh(const long bsize[3], const long dims[KSPACE_DIMS], complex float* sens, const long caldims[KSPACE_DIMS], const complex float* data)
{
	assert(1 == caldims[MAPS_DIM]);
	assert(1 == dims[MAPS_DIM]);

	int channels = caldims[COIL_DIM];
	int cosize = channels * (channels + 1) / 2;
	assert(dims[COIL_DIM] == cosize);

	long dims1[KSPACE_DIMS];
	md_copy_dims(KSPACE_DIMS, dims1, dims);
	dims1[COIL_DIM] = channels;
	
	long kdims[4];
	kdims[0] = MIN(bsize[0], dims[0]);
	kdims[1] = MIN(bsize[1], dims[1]);
	kdims[2] = MIN(bsize[2], dims[2]);

	md_resizec(KSPACE_DIMS, dims1, sens, caldims, data, sizeof(complex float));
	ifftc(KSPACE_DIMS, dims1, FFT_FLAGS, sens, sens);

	long odims[KSPACE_DIMS];
	md_copy_dims(KSPACE_DIMS, odims, dims1);

	for (int i = 0; i < 3; i++)
		odims[i] = dims[i] + kdims[i] - 1;

	complex float* tmp = md_alloc(KSPACE_DIMS, odims, sizeof(complex float));
	md_resizec(KSPACE_DIMS, odims, tmp, dims1, sens, sizeof(complex float));

	// FIXME: we should have the option to compute this from a periodic 
	// extension 

	long calmat_dims[2];
	complex float* cm = calibration_matrix(calmat_dims, kdims, odims, tmp);
	md_free(tmp);

	int xh = dims[0];
	int yh = dims[1];
	int zh = dims[2];

	int pixels = calmat_dims[1] / channels;

#pragma omp parallel for
	for (int k = 0; k < zh; k++) {

		complex float in[channels][pixels];
		complex float cov[cosize];

		for (int j = 0; j < yh; j++) {
			for (int i = 0; i < xh; i++) {

				for (int c = 0; c < channels; c++)
					for (int p = 0; p < pixels; p++)
						in[c][p] = cm[((((c * pixels + p) * zh) + k) * yh + j) * xh + i];

				gram_matrix2(channels, cov, pixels, (const complex float (*)[])in);

				for (int l = 0; l < cosize; l++)
					sens[(((l * zh) + k) * yh + j) * xh + i] = cov[l];
			}
		}
	}
}



