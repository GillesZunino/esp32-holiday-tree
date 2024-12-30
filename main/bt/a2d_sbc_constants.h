// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once


// SBC codec sampling frequencies
#define A2D_SBC_IE_SAMP_FREQ_MSK    0xF0    // b7-b4 sampling frequency
#define A2D_SBC_IE_SAMP_FREQ_16     0x80    // b7: 16  kHz
#define A2D_SBC_IE_SAMP_FREQ_32     0x40    // b6: 32  kHz
#define A2D_SBC_IE_SAMP_FREQ_44     0x20    // b5: 44.1kHz
#define A2D_SBC_IE_SAMP_FREQ_48     0x10    // b4: 48  kHz

// SBC channel mode
#define A2D_SBC_IE_CH_MD_MSK        0x0F    // b3-b0 channel mode
#define A2D_SBC_IE_CH_MD_MONO       0x08    // b3: mono
#define A2D_SBC_IE_CH_MD_DUAL       0x04    // b2: dual
#define A2D_SBC_IE_CH_MD_STEREO     0x02    // b1: stereo
#define A2D_SBC_IE_CH_MD_JOINT      0x01    // b0: joint stereo

// SBC number of blocks
#define A2D_SBC_IE_BLOCKS_MSK       0xF0    // b7-b4 number of blocks
#define A2D_SBC_IE_BLOCKS_4         0x80    // 4 blocks
#define A2D_SBC_IE_BLOCKS_8         0x40    // 8 blocks
#define A2D_SBC_IE_BLOCKS_12        0x20    // 12 blocks
#define A2D_SBC_IE_BLOCKS_16        0x10    // 16 blocks

// SBC sub bands
#define A2D_SBC_IE_SUBBAND_MSK      0x0C    // b3-b2 number of subbands
#define A2D_SBC_IE_SUBBAND_4        0x08    // b3: 4
#define A2D_SBC_IE_SUBBAND_8        0x04    // b2: 8

// SBC allocation mode
#define A2D_SBC_IE_ALLOC_MD_MSK     0x03    // b1-b0 allocation mode
#define A2D_SBC_IE_ALLOC_MD_S       0x02    // b1: SNR
#define A2D_SBC_IE_ALLOC_MD_L       0x01    // b0: loudness