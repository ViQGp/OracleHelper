/*
 *  kissfft - Lightweight FFT (BSD-3-Clause)
 *
 *  Minimal, self-contained single-precision FFT sufficient for
 *  real-valued FFT of power-of-2 sizes (we use 2048).
 *
 *  Original kissfft by Mark Borgerding.
 *  Trimmed and adapted for OracleHelper.
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

typedef float kiss_fft_scalar;

typedef struct {
    kiss_fft_scalar r;
    kiss_fft_scalar i;
} kiss_fft_cpx;

/* Internal config */
typedef struct kiss_fft_state {
    int nfft;
    int inverse;
    int *factors;
    kiss_fft_cpx *twiddles;
} *kiss_fft_cfg;

/* Real FFT config = same as regular cfg */
typedef kiss_fft_cfg kiss_fftr_cfg;

/* ---------------- helpers ---------------- */
static void kf_factor(int n, int *facbuf) {
    int p = 4, nf = 0, ntmp = n, q = 0;
    while (ntmp > 1) {
        int found = 0;
        while (!found) {
            q = ntmp / p;
            int r = ntmp % p;
            if (r == 0) { found = 1; break; }
            switch (p) {
                case 4: p = 2; break;
                case 2: p = 3; break;
                default: p += 2;
            }
            if (p > 0x7fffffff) { p = 0; found = 1; }
        }
        if (p == 0) { facbuf[nf++] = ntmp; break; }
        ntmp = q;
        facbuf[nf++] = p;
    }
    facbuf[nf] = 0;
}

static kiss_fft_cfg kf_alloc(int nfft, int inverse) {
    kiss_fft_cfg cfg = (kiss_fft_cfg)malloc(sizeof(*cfg));
    if (!cfg) return NULL;
    cfg->nfft = nfft;
    cfg->inverse = inverse;
    cfg->factors = (int *)malloc(sizeof(int) * 32);
    kf_factor(nfft, cfg->factors);

    int twiddle_size = nfft - 1;
    cfg->twiddles = (kiss_fft_cpx *)malloc(sizeof(kiss_fft_cpx) * twiddle_size);

    for (int i = 0; i < twiddle_size; ++i) {
        double phase = (inverse ? 2 : -2) * M_PI * (double)i / (double)nfft;
        cfg->twiddles[i].r = (float)cos(phase);
        cfg->twiddles[i].i = (float)sin(phase);
    }
    return cfg;
}

/* ---------------- public API ---------------- */
kiss_fft_cfg kiss_fft_alloc(int nfft, int inverse_fft, void *mem, size_t *lenmem) {
    (void)mem; (void)lenmem;
    return kf_alloc(nfft, inverse_fft);
}

void kiss_fft_free(kiss_fft_cfg cfg) {
    if (cfg) {
        free(cfg->twiddles);
        free(cfg->factors);
        free(cfg);
    }
}

/* In-place iterative Cooley-Tukey for power-of-2 */
void kiss_fft(kiss_fft_cfg cfg, const kiss_fft_cpx *fin, kiss_fft_cpx *fout) {
    int n = cfg->nfft;

    /* bit-reversal copy */
    for (int i = 0; i < n; ++i) {
        int j = 0, t = i, b = 0;
        int tmp = n;
        while (tmp > 1) { b++; tmp >>= 1; }
        for (int k = 0; k < b; ++k) {
            j = (j << 1) | (t & 1);
            t >>= 1;
        }
        fout[j] = fin[i];
    }

    /* iterative FFT */
    for (int size = 2; size <= n; size *= 2) {
        int half = size / 2;
        for (int i = 0; i < n; i += size) {
            for (int k = 0; k < half; ++k) {
                int tw_idx = (k * n / size);
                if (cfg->inverse) {
                    tw_idx = (-tw_idx % n + n) % n;
                } else {
                    tw_idx = tw_idx % n;
                }
                /* skip the 0th twiddle (== 1+0i) by using array offset */
                kiss_fft_cpx w;
                if (tw_idx == 0) {
                    w.r = 1.0f; w.i = 0.0f;
                } else {
                    /* twiddles[] starts at index 0 = 1*step */
                    int idx = (tw_idx - 1 + n - 1) % (n - 1);
                    w.r = cfg->twiddles[idx].r;
                    w.i = cfg->twiddles[idx].i;
                    if (cfg->inverse) w.i = -w.i;
                }

                kiss_fft_cpx t, u;
                t.r = fout[i + k + half].r * w.r - fout[i + k + half].i * w.i;
                t.i = fout[i + k + half].r * w.i + fout[i + k + half].i * w.r;
                u = fout[i + k];
                fout[i + k].r     = u.r + t.r;
                fout[i + k].i     = u.i + t.i;
                fout[i + k + half].r = u.r - t.r;
                fout[i + k + half].i = u.i - t.i;
            }
        }
    }

    if (cfg->inverse) {
        for (int i = 0; i < n; ++i) {
            fout[i].r /= n;
            fout[i].i /= n;
        }
    }
}

/* ---------------- real FFT wrappers ---------------- */
kiss_fftr_cfg kiss_fftr_alloc(int nfft, int inverse, void *mem, size_t *lenmem) {
    (void)mem; (void)lenmem;
    return (kiss_fftr_cfg)kf_alloc(nfft, inverse);
}

void kiss_fftr(kiss_fftr_cfg cfg, const kiss_fft_scalar *timedata, kiss_fft_cpx *freqdata) {
    int n = ((kiss_fft_cfg)cfg)->nfft;
    int nc = n / 2 + 1;
    kiss_fft_cpx *tmp = (kiss_fft_cpx *)malloc(sizeof(kiss_fft_cpx) * nc);
    for (int i = 0; i < n; ++i) {
        int idx = (i < nc) ? i : (i - nc);
        if (idx < 0) idx += nc;
        tmp[idx].r = timedata[i];
        tmp[idx].i = 0;
    }
    kiss_fft((kiss_fft_cfg)cfg, tmp, freqdata);
    free(tmp);
}

void kiss_fftri(kiss_fftr_cfg cfg, const kiss_fft_cpx *freqdata, kiss_fft_scalar *timedata) {
    int n = ((kiss_fft_cfg)cfg)->nfft;
    kiss_fft_cpx *tmp = (kiss_fft_cpx *)malloc(sizeof(kiss_fft_cpx) * n);
    kiss_fft((kiss_fft_cfg)cfg, freqdata, tmp);
    for (int i = 0; i < n; ++i) timedata[i] = tmp[i].r;
    free(tmp);
}

void kiss_fftr_free(kiss_fftr_cfg cfg) {
    kiss_fft_free((kiss_fft_cfg)cfg);
}
