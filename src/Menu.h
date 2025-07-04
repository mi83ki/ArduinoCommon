/***********************************************************************/
/*                                                                     */
/*  FILE        :Menu.h                                                */
/*  DATE        :Apl 21, 2021                                          */
/*  DESCRIPTION :メニュー表示クラス                                    */
/*                                                                     */
/*  This file is generated by Tatsuya Miyazaki                         */
/*                                                                     */
/***********************************************************************/

#pragma once

#include <Arduino.h>

/***********************************************************************/
/*                         メニュー表示クラス                          */
/***********************************************************************/
/***********************************************************************/
/*                                                                     */
/*  FILE        :menu.c                                                */
/*  DATE        :Wed, Dec 28, 2010                                     */
/*  DESCRIPTION :動作メニューを管理するソース                          */
/*  CPU TYPE    :SH7047                                                */
/*                                                                     */
/*  This file is generated by Tatsuya Miyazaki.                        */
/*                                                                     */
/***********************************************************************/

/***************************  使い方  **********************************

・ポイント
 menu機能を利用することができる．

・使い方
  ①menu.cをインクルードしているソース内で
    menuType の配列と，それに対応する関数を書く．
  ②main関数のwhile内で，menuApp() 関数を実行する．

・例(spaceキーを押せばメニューを表示させる様に設定する場合)
---------------------------------------------------------------------
#include "Menu.hpp"

// 関数の引数は必ず"uint8_t *md"にする．
void function1(uint8_t *md);
void function2(uint8_t *md);
void function3(uint8_t *md);
void experiment1(uint8_t *md);
void experiment2(uint8_t *md);

const Menu::menuType menu[] = {
  // 最初にあった方がいい
  {Menu::IDLE, "Idle process."           },

  // ここからがユーザーの変える部分
  {function1, "Add 2 to the 1.",       'a'},
  {function2, "Subtract 1 from 2.",    'b'},
  {function3, "Multiply 1 and 2..",    'c'},
  {experiment1, "Print the count1."       },
  {experiment2, "Print the count2."       },

  // 必ず最後に必要
  {Menu::END, "",                    '\0'}
};

static unsigned int count1, count2;

int main(void) {
  char command = 0;

  // spaceキーでmenu表示に設定
  Menu menu = Menu(menu, ' ');

  while(1) {
    count1++;
    if (checkmystdin()) {
      count2++;
      command = mygetchar();
    }
    menu.menuApp(command);
  }
}

void function1(uint8_t *md) {
  myprintf("%d + %d = %d\n\r", 1, 2, 1+2);
  *md = 0;   // 1度しか実行しなくてよい時は0にクリア
}

void function2(uint8_t *md) {
  myprintf("%d - %d = %d\n\r", 1, 2, 1-2);
  *md = 0;
}

void function3(uint8_t *md) {
  myprintf("%d * %d = %d\n\r", 1, 2, 1*2);
  *md = 0;
}

void experiment1(uint8_t *md) {
  if (cmtFlag) {
    cmtFlag = 0;
    myprintf("%d\n\r", count1);
  }
}

void experiment2(uint8_t *md) {
  if (cmtFlag) {
    cmtFlag = 0;
    myprintf("%d\n\r", count2);
  }
}
---------------------------------------------------------------------

・解説
 ①menuType は{関数のアドレス，出力メッセージ，ショートカットキー}
   で構成される．
 ②menuApp() の引数は，PCからの入力コマンド．
 ③initMenu() の引数はメニュー配列とメニューを表示させるキーである．
   例の場合，spaceキーを押せばずらっとメニューが表示される．

**********************************************************************/

class Menu {
 public:
  typedef struct menu {
    void (*function)(uint8_t *); /* 実行関数のアドレス */
    char name[60];               /* 実行関数の説明 */
    char shortcutkey;            /* ショートカットキー（あれば） */
  } menuType;
  Menu(const menuType *menu, char menuChar);
  ~Menu();
  static String mygets(void);
  static void flush(void);
  uint8_t menuApp(char com);
  void printShortcuts(void);
  uint8_t isFirstAccess(uint8_t md);
  uint8_t getLastMode(void);
  static void IDLE(uint8_t *md) { *md = 0; }
  static void END(uint8_t *md) { *md = 0; }

 private:
  uint8_t countMenu(const menuType me[]);
  uint8_t getMenuNum(void (*f)(uint8_t *));
  void scanMode(uint8_t *md);
  void getMode(uint8_t *md, char com);
  void exeMode(uint8_t *md);

 private:
  /* どのキーを押せばメニューを表示させるのか */
  char gMenuChar;
  /* メニュー配列のアドレス格納ポインタ */
  const menuType *gMenu;
  /* メニュー配列の個数 */
  uint8_t gMenuNum;

  uint8_t lastMode;
};