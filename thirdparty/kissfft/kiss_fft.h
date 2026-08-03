/*
 *  kiss_fft.h - Lightweight FFT header
 *  BSD-3-Clause
 */
#ifndef KISS_FFT_H
#define KISS_FFT_H

#include <stdlib.h>
#include <math.h>

typedef float kiss_fft_scalar;

typedef struct {
    kiss_fft_scalar r;
    kiss_fft_scalar i;
} kiss_fft_cpx;

typedef struct kiss_fft_state *kiss_fft_cfg;

typedef struct kiss_fftr_state *kiss_fftr_cfg;

kiss_fft_cfg kiss_fft_alloc(int nfft, int inverse_fft, void *mem, size_t *lenmem);
void kiss_fft(kiss_fft_cfg cfg, const kiss_fft_cpx *fin, kiss_fft_cpx *fout);
void kiss_fft_free(kiss_fft_cfg cfg);

kiss_fftr_cfg kiss_fftr_alloc(int nfft, int inverse, void *mem, size_t *lenmem);
void kiss_fftr(kiss_fftr_cfg cfg, const kiss_fft_scalar *timedata, kiss_fft_cpx *freqdata);
void kiss_fftri(kiss_fftr_cfg cfg, const kiss_fft_cpx *freqdata, kiss_fft_scalar *timedata);
void kiss_fftr_free(kiss_fftr_cfg cfg);

#endif /* KISS_FFT_H */
