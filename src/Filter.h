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

#include "fix.hpp"

/***********************************************************************/
/*                      1次のフィルタリング関数                        */
/***********************************************************************/

// 列挙型
enum eFILT_MODE
{
  LPF,
  HPF
};

// クラス
class FirstFilter
{
public:
  FirstFilter(enum eFILT_MODE, float, uint16_t, fix x0 = 0);
  fix getLPF(void);
  void setLPF(fix);
  fix getOut(void);
  fix firstFiltering(fix);

private:
  fix calcTC(float, uint16_t);
  float calcFREQ(fix, uint16_t);
  /** フィルタ後の出力値 */
  fix _out;
  /** ローパスかハイパスか */
  enum eFILT_MODE _mode;
  /** 時定数 */
  fix _tc;
  /** ローパスフィルタ値 */
  fix _lpf;
};

/***********************************************************************/
/*                          移動平均フィルタ                           */
/***********************************************************************/

// クラス
class MovAveFilter
{
public:
  MovAveFilter(uint8_t, fix x0 = 0);
  ~MovAveFilter();
  void setData(fix);
  fix movingAverage(fix);
  fix getOut(void);

private:
  /** 移動平均出力値 */
  fix _out;
  /** 平均をとる個数 */
  uint8_t _size;
  /** リングバッファ用現在値 */
  uint8_t _now;
  /** 過去のデータを記憶しておくバッファ */
  fix *_data;
  /** 合計 */
  int64_t _sum;
};

/***********************************************************************/
/*                          移動最大フィルタ                           */
/***********************************************************************/

class MovMaxFilter
{
public:
  MovMaxFilter(uint8_t, float x0 = 0);
  ~MovMaxFilter();
  void setData(float);
  float movingMax(float);
  float getOut(void);

private:
  /** 平均をとる個数 */
  uint8_t _size;
  /** リングバッファ用現在値 */
  uint8_t _now;
  /** 過去のデータを記憶しておくバッファ */
  float *_data;
  /** 最大値 */
  float _out;
};
