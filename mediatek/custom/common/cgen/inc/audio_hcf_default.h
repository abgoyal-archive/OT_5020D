

#ifndef AUDIO_HCF_DEFAULT_H
#define AUDIO_HCF_DEFAULT_H

/* Compensation Filter HSF coeffs: default all pass filter       */
/* BesLoudness also uses this coeffs    */
#define BES_LOUDNESS_HCF_HSF_COEFF \
    0, 0, 0, 0, \
    0, 0, 0, 0, \
    0, 0, 0, 0, \
    0, 0, 0, 0, \
    0, 0, 0, 0, \
    0, 0, 0, 0, \
    0, 0, 0, 0, \
    0, 0, 0, 0, \
    0, 0, 0, 0

/* Compensation Filter BPF coeffs: default all pass filter      */
#define BES_LOUDNESS_HCF_BPF_COEFF \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    \     
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    \     
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    \     
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0, \
    0, 0, 0

#define BES_LOUDNESS_HCF_DRC_FORGET_TABLE \
    0, 0, \
    0, 0, \
    0, 0, \
    0, 0, \
    0, 0, \
    0, 0, \
    0, 0, \
    0, 0, \
    0, 0

#define BES_LOUDNESS_HCF_WS_GAIN_MAX  0

#define BES_LOUDNESS_HCF_WS_GAIN_MIN  0

#define BES_LOUDNESS_HCF_FILTER_FIRST  0

#define BES_LOUDNESS_HCF_GAIN_MAP_IN \
    0, 0, 0, 0, 0

#define BES_LOUDNESS_HCF_GAIN_MAP_OUT \
    0, 0, 0, 0, 0

#endif
