#include "NeoPixelArrayBase.h"

/**
 * @file NeoPixelArrayBase.cpp
 * @brief FastLED を使った汎用 NeoPixel 配列制御の共通基底クラス実装
 */

/**
 * @brief NeoPixel 配列制御の共通基底クラスを初期化する
 *
 * @param num LED 数
 */
NeoPixelArrayBase::NeoPixelArrayBase(uint16_t num)
    : _num(num),
      _colors(new CRGB[num]()),
      _rainbowTimer(10),
      _firstPixelHue(0),
      _changed(false),
      _forceDisable(false) {}

/**
 * @brief NeoPixel 配列制御の共通基底クラスを破棄する
 */
NeoPixelArrayBase::~NeoPixelArrayBase() { delete[] _colors; }

/**
 * @brief すべての LED を同じ色で塗りつぶす
 *
 * @param color 24bit RGB カラー
 */
void NeoPixelArrayBase::fillAll(uint32_t color) { fillAll(CRGB(color)); }

/**
 * @brief 指定範囲の LED を同じ色で塗りつぶす
 *
 * @param color 24bit RGB カラー
 * @param first 開始インデックス
 * @param count 塗りつぶす LED 数
 */
void NeoPixelArrayBase::fill(uint32_t color, uint16_t first, uint16_t count) {
  fill(CRGB(color), first, count);
}

/**
 * @brief 指定した LED を設定する
 *
 * @param index LED インデックス
 * @param color 24bit RGB カラー
 */
void NeoPixelArrayBase::setPixelColor(uint16_t index, uint32_t color) {
  setPixelColor(index, CRGB(color));
}

/**
 * @brief すべての LED を同じ色で塗りつぶす
 *
 * @param color FastLED のカラー
 */
void NeoPixelArrayBase::fillAll(const CRGB& color) { fill(color, 0, _num); }

/**
 * @brief 指定範囲の LED を同じ色で塗りつぶす
 *
 * @param color FastLED のカラー
 * @param first 開始インデックス
 * @param count 塗りつぶす LED 数
 */
void NeoPixelArrayBase::fill(const CRGB& color, uint16_t first,
                             uint16_t count) {
  if (first >= _num || count == 0) {
    return;
  }

  uint32_t end = static_cast<uint32_t>(first) + static_cast<uint32_t>(count);
  if (end > _num) {
    end = _num;
  }

  for (uint16_t i = first; i < end; ++i) {
    if (_colors[i] != color) {
      _colors[i] = color;
      _changed = true;
    }
  }
}

/**
 * @brief 指定した LED を設定する
 *
 * @param index LED インデックス
 * @param color FastLED のカラー
 */
void NeoPixelArrayBase::setPixelColor(uint16_t index, const CRGB& color) {
  if (index >= _num) {
    return;
  }

  if (_colors[index] != color) {
    _colors[index] = color;
    _changed = true;
  }
}

/**
 * @brief 変更済みフラグを取得し、取得時にフラグをクリアする
 *
 * @return true LED 設定が更新されている
 * @return false LED 設定に変更がない
 */
bool NeoPixelArrayBase::isChanged(void) {
  if (_changed) {
    _changed = false;
    return true;
  }
  return false;
}

/**
 * @brief 変更があるときだけ FastLED.show() を実行する
 *
 * @return true 表示更新した
 * @return false 表示更新していない
 */
bool NeoPixelArrayBase::update(void) {
  if (_forceDisable) {
    return false;
  }

  if (isChanged()) {
    FastLED.show();
    return true;
  }
  return false;
}

/**
 * @brief 指定した待ち時間でレインボー表示を更新する
 *
 * @param wait 更新周期[ms]
 * @param numPixel レインボー対象の LED 数
 */
void NeoPixelArrayBase::rainbow(int wait, uint8_t numPixel) {
  if (numPixel == 0) {
    return;
  }

  _rainbowTimer.setCycleTime(wait);
  if (_rainbowTimer.isCycleTime()) {
    _firstPixelHue = static_cast<uint16_t>((_firstPixelHue + 1) % 256);
    for (uint8_t i = 0; i < numPixel; ++i) {
      uint8_t pixelHue =
          static_cast<uint8_t>((_firstPixelHue + (i * 256 / numPixel)) % 256);
      setPixelColor(i, CHSV(pixelHue, 255, 255));
    }
  }
}

/**
 * @brief 強制無効化状態を切り替える
 *
 * @param forceDisable true のとき update() を停止する
 */
void NeoPixelArrayBase::setForceDisable(bool forceDisable) {
  _forceDisable = forceDisable;
  fillAll(0);
  FastLED.show();
}

/**
 * @brief 管理している LED 数を返す
 *
 * @return uint16_t LED 数
 */
uint16_t NeoPixelArrayBase::size(void) const { return _num; }

/**
 * @brief 現在保持している LED 色を取得する
 *
 * @param index LED インデックス
 * @return uint32_t 24bit RGB カラー
 */
uint32_t NeoPixelArrayBase::getPixelColor(uint16_t index) const {
  if (index >= _num) {
    return 0;
  }
  return toColor(_colors[index]);
}

/**
 * @brief 指定色に輝度を掛けた色を返す
 *
 * @param inColor 24bit RGB カラー
 * @param brightness 0-255 の輝度
 * @return uint32_t 輝度適用後の 24bit RGB カラー
 */
uint32_t NeoPixelArrayBase::setBrightness(uint32_t inColor,
                                          uint8_t brightness) {
  uint8_t outRed = static_cast<uint16_t>(red(inColor)) * brightness / 255;
  uint8_t outGreen = static_cast<uint16_t>(green(inColor)) * brightness / 255;
  uint8_t outBlue = static_cast<uint16_t>(blue(inColor)) * brightness / 255;
  return toColor(outRed, outGreen, outBlue);
}

/**
 * @brief 最大成分が 255 になるように色を正規化する
 *
 * @param inColor 24bit RGB カラー
 * @return uint32_t 正規化後の 24bit RGB カラー
 */
uint32_t NeoPixelArrayBase::setFullBrightness(uint32_t inColor) {
  uint8_t inRed = red(inColor);
  uint8_t inGreen = green(inColor);
  uint8_t inBlue = blue(inColor);
  uint8_t maxValue = max(inRed, max(inGreen, inBlue));

  if (maxValue == 0) {
    return inColor;
  }

  uint8_t outRed = static_cast<uint16_t>(inRed) * 255 / maxValue;
  uint8_t outGreen = static_cast<uint16_t>(inGreen) * 255 / maxValue;
  uint8_t outBlue = static_cast<uint16_t>(inBlue) * 255 / maxValue;
  return toColor(outRed, outGreen, outBlue);
}

/**
 * @brief 0-100[%] 指定の輝度で色を生成する
 *
 * @param inColor 元の 24bit RGB カラー
 * @param brightnessPercent 輝度[%]
 * @return uint32_t 輝度適用後の 24bit RGB カラー
 */
uint32_t NeoPixelArrayBase::getRGB(uint32_t inColor,
                                   uint8_t brightnessPercent) {
  uint8_t clampedBrightness =
      brightnessPercent > 100 ? 100 : brightnessPercent;
  uint32_t outColor = setFullBrightness(inColor);
  return setBrightness(outColor, 255 * clampedBrightness / 100);
}

/**
 * @brief 補色を返す
 *
 * @param color 24bit RGB カラー
 * @return uint32_t 補色
 */
uint32_t NeoPixelArrayBase::getComplementaryColor(uint32_t color) {
  uint8_t inRed = red(color);
  uint8_t inGreen = green(color);
  uint8_t inBlue = blue(color);
  uint8_t maxValue = max(inRed, max(inGreen, inBlue));
  uint8_t minValue = min(inRed, min(inGreen, inBlue));

  return toColor(static_cast<uint8_t>(maxValue + minValue - inRed),
                 static_cast<uint8_t>(maxValue + minValue - inGreen),
                 static_cast<uint8_t>(maxValue + minValue - inBlue));
}

/**
 * @brief FastLED の全体輝度を設定する
 *
 * @param brightness 0-255 の輝度
 */
void NeoPixelArrayBase::setBrightnessLevel(uint8_t brightness) {
  FastLED.setBrightness(brightness);
}

/**
 * @brief 管理している CRGB 配列を返す
 *
 * @return CRGB* CRGB 配列
 */
CRGB* NeoPixelArrayBase::colors(void) { return _colors; }

/**
 * @brief 管理している CRGB 配列を返す
 *
 * @return const CRGB* CRGB 配列
 */
const CRGB* NeoPixelArrayBase::colors(void) const { return _colors; }

uint8_t NeoPixelArrayBase::red(uint32_t color) {
  return static_cast<uint8_t>((color >> 16) & 0xFF);
}

uint8_t NeoPixelArrayBase::green(uint32_t color) {
  return static_cast<uint8_t>((color >> 8) & 0xFF);
}

uint8_t NeoPixelArrayBase::blue(uint32_t color) {
  return static_cast<uint8_t>(color & 0xFF);
}

uint32_t NeoPixelArrayBase::toColor(uint8_t red, uint8_t green, uint8_t blue) {
  return (static_cast<uint32_t>(red) << 16) |
         (static_cast<uint32_t>(green) << 8) | static_cast<uint32_t>(blue);
}

uint32_t NeoPixelArrayBase::toColor(const CRGB& color) {
  return toColor(color.r, color.g, color.b);
}
