/***********************************************************************/
/*                                                                     */
/*  FILE        :Filter.h                                             */
/*  DATE        :Sun, May 30, 2010                                     */
/*  DESCRIPTION :Filtering Program Header                              */
/*                                                                     */
/*  This file is generated by Tatsuya Miyazaki.                        */
/*                                                                     */
/***********************************************************************/
/*                               使い方                                */
/***********************************************************************/
/***********************************************************************/
/*                         一次フィルタの場合                          */
/***********************************************************************/
/*                                                                     */
/* ①フィルターしたい変数を firstfilterType x1; のように定義する。     */
/* ②その変数を initFirstFilter() で初期化する。                       */
/* ③CYCLE_TIME の周期で firstFiltering() する。                       */
/* ④x1.out がフィルタ後の値。                                         */
/*                                                                     */
/*---------------------------------------------------------------------*/
/*      例 (while内のif文がCYCLE_TIMEの周期で実行されるようにする)    */
/*---------------------------------------------------------------------*/
/*                                                                     */
/* int main(void) {                                                    */
/*   fix a;   // センサのA/D変化値等，時々刻々変化する値とする         */
/*                                                                     */
/*   // x1を高域遮断周波数2.0Hzのローパスフィルタに設定                */
/*   // フィルタリングを実行する周期が10msの場合                       */
/*   FirstFilter x1(LPF, 2.0, 10);                                    */
/*                                                                     */
/*   // x2を低域遮断周波数1.0Hzのハイパスフィルタに設定                */
/*   // フィルタリングを実行する周期が10msの場合                       */
/*   FirstFilter x2(HPF, 1.0, 10);                                    */
/*                                                                     */
/*   while (1) {                                                       */
/*     if (CYCLE_TIMEごとに真) {                                       */
/*       a = getAD();        // センサの電圧をA/D変換してaに代入(例)   */
/*       x1.firstFiltering(a);    // aをローパスフィルタx1にかける     */
/*       x2.firstFiltering(a);    // aをハイパスフィルタx2にかける     */
/*       printf("%f, %f, %f\n\r",                                      */
/*              FIX_TO_FLOAT(a),                                       */
/*              FIX_TO_FLOAT(x1.getOut()),                             */
/*              FIX_TO_FLOAT(x2.getOut()));  // 出力                   */
/*     }                                                               */
/*   }                                                                 */
/*   return(0);                                                        */
/* }                                                                   */
/*                                                                     */
/***********************************************************************/
#pragma once

#include "fix.h"

/***********************************************************************/
/*                      1次のフィルタリング関数                        */
/***********************************************************************/

// 列挙型
enum eFILT_MODE
{
  LPF,
  HPF
};

// 構造体
typedef struct firstfilter
{                       /* フィルタを使う変数の構造体 */
  fix out;              /* フィルタ後の出力値 */
  enum eFILT_MODE mode; /* ローパスかハイパスか */
  fix tc;               /* 時定数 */
  fix lpf;              /* ローパスフィルタ値 */
} firstFilterType;

// クラス
class FirstFilter
{
public:
  FirstFilter(enum eFILT_MODE fimo, float freq, uint16_t cycleTime, fix x0 = 0);
  fix getLPF(void);
  fix getOut(void);
  fix firstFiltering(fix in);

private:
  firstFilterType m_Filter;
  fix calcTC(float freq, uint16_t cycleTime);
  float calcFREQ(fix tc, uint16_t cycleTime);
};

/***********************************************************************/
/*                          移動平均フィルタ                           */
/***********************************************************************/

// クラス
class MovAveFilter
{
public:
  MovAveFilter(uint8_t size, fix x0);
  ~MovAveFilter();
  fix movingAverage(fix xn);
  fix getOut(void);

private:
  // 構造体
  struct movave
  {
    fix out;
    fix *data;    /* 過去のデータを記憶しておくバッファ */
    int64_t sum;  /* 合計 */
    uint8_t size; /* 平均をとる個数 */
    uint8_t now;  /* リングバッファ用現在値 */
  } m_Filter;
};
