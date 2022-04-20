/***********************************************************************/
/*                                                                     */
/*  FILE        :Filter.c                                             */
/*  DATE        :Sat, Jul 4, 2020                                      */
/*  DESCRIPTION :Filtering Program                                     */
/*                                                                     */
/*  This file is generated by Tatsuya Miyazaki.                        */
/*                                                                     */
/***********************************************************************/

#ifndef _CFILTER_CPP__
#define _CFILTER_CPP__

#include <math.h>
#include "Filter.h"

/***********************************************************************/
/*                      1次のフィルタリング関数                        */
/***********************************************************************/
#ifndef MY_PI
#define MY_PI (3.14159276f)
#endif

/* フィルタの時定数を計算する関数 */
fix CFirstFilter::calcTC(float freq, uint16_t cycleTime)
{
  freq = 1.0 - expf(-2.0 * MY_PI * freq * (float)cycleTime / 1000.0f);
  return (FLOAT_TO_FIX(freq));
}

/* 時定数からカットオフ周波数を計算 */
float CFirstFilter::calcFREQ(fix tc, uint16_t cycleTime)
{
  float temp = FIX_TO_FLOAT(tc);
  temp = 1.0 - temp;
  return (-logf(temp) * 1000.0f / (float)cycleTime / 2.0 / MY_PI);
}

/* フィルタを使う構造体の初期化 */
CFirstFilter::CFirstFilter(enum eFILT_MODE fimo, float freq, uint16_t cycleTime, fix x0)
{
  m_Filter.out = 0;
  m_Filter.mode = fimo;
  m_Filter.tc = calcTC(freq, cycleTime);
  m_Filter.lpf = x0;
}

// ローパスフィルタ値を返す
fix CFirstFilter::getLPF(void)
{
  return (m_Filter.lpf);
}

// フィルタ出力値を返す
fix CFirstFilter::getOut(void)
{
  return (m_Filter.out);
}

/* 1次フィルタをかける関数 */
fix CFirstFilter::firstFiltering(fix in)
{
  m_Filter.lpf += FIX_MUL(in - m_Filter.lpf, m_Filter.tc);
  if (m_Filter.mode == LPF)
    m_Filter.out = m_Filter.lpf;
  else if (m_Filter.mode == HPF)
    m_Filter.out = in - m_Filter.lpf;
  else
    m_Filter.out = 0;
  return (m_Filter.out);
}

/***********************************************************************/
/*                          移動平均フィルタ                           */
/***********************************************************************/

#define NEXT(n, size) (((n) + 1) % (size))
#define BEFORE(n, size) (((n) + (size)-1) % (size))

CMovAveFilter::CMovAveFilter(uint8_t size, fix x0)
{
  uint8_t i;
  m_Filter.out = 0;
  m_Filter.data = new fix[size];
  m_Filter.size = size;
  m_Filter.now = 0;
  for (i = 0; i < m_Filter.size; i++)
  {
    m_Filter.data[i] = x0;
  }
  m_Filter.sum = (int64_t)x0 * (int64_t)size;
}

CMovAveFilter::~CMovAveFilter()
{
  delete[] m_Filter.data;
}

fix CMovAveFilter::movingAverage(fix xn)
{
  m_Filter.sum -= (int64_t)m_Filter.data[m_Filter.now]; /* 一番古いのを消して */
  m_Filter.sum += (int64_t)xn;                          /* 一番新しいのを足す */
  m_Filter.data[m_Filter.now] = xn;                     /* バッファに書きこむ */
  m_Filter.now = NEXT(m_Filter.now, m_Filter.size);

  m_Filter.out = (fix)(m_Filter.sum / (int64_t)m_Filter.size);
  return (m_Filter.out);
}

// フィルタ出力値を返す
fix CMovAveFilter::getOut(void)
{
  return (m_Filter.out);
}

#endif // _CFILTER_CPP__
