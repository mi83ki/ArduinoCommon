/***********************************************************************/
/*                                                                     */
/*  FILE        :fix.hpp                                               */
/*  DATE        :Tue, Nov 9, 2010                                      */
/*  DESCRIPTION :固定小数点演算ライブラリヘッダ                        */
/*  CPU TYPE    :SH7047                                                */
/*                                                                     */
/*  This file is generated by Tatsuya Miyazaki                         */
/*                                                                     */
/***********************************************************************/
#pragma once

#include <arduino.h>

typedef int32_t fix;

/* 固定少数点の位置（ビット）*/
#define FIX_SHIFT_BIT (16)
/* 固定少数点の位置（値）*/
#define FIX_SHIFT_VAL (1 << FIX_SHIFT_BIT)
#define FIX_SHIFT_VALF (65536.0f)
/* 小数部分マスク値 */
#define FIX_DEC_MASK (FIX_SHIFT_VAL - 1.0f)
/* HALF = 0.5 */
#define FIX_HALF (1 << (FIX_SHIFT_BIT - 1))

/* 浮動小数点から固定小数点へ */
#define FLOAT_TO_FIX(ft) ((fix)((ft)*FIX_SHIFT_VALF))
/* 固定小数点から浮動小数点へ */
#define FIX_TO_FLOAT(fx) (((float)(fx)) / FIX_SHIFT_VALF)
/* 整数から固定小数点を作る */
#define INT_TO_FIX(i) (((fix)(i)) << FIX_SHIFT_BIT)
/* 固定小数点から整数へ */
#define FIX_TO_INT(fx) (((fx) + FIX_HALF) >> FIX_SHIFT_BIT)

/* 固定小数点数の掛け算(int64_t でキャスト) */
#define FIX_MUL(fx1, fx2) ((fix)((((int64_t)(fx1)) * ((int64_t)(fx2))) >> FIX_SHIFT_BIT))
/* 固定小数点数の割り算(int64_t でキャスト) */
#define FIX_DIV(fx1, fx2) ((fix)((((int64_t)(fx1)) << FIX_SHIFT_BIT) / ((int64_t)(fx2))))
/* 固定小数点数の掛け算(精度落ちるが速い) */
#define FIX_MUL2(fx1, fx2) (((fx1) >> (FIX_SHIFT_BIT >> 1)) * ((fx2) >> (FIX_SHIFT_BIT >> 1)))
/* 固定小数点数の割り算(精度落ちるが速い) */
#define FIX_DIV2(fx1, fx2) (((fx1) << (FIX_SHIFT_BIT >> 1)) / ((fx2) >> (FIX_SHIFT_BIT >> 1)))