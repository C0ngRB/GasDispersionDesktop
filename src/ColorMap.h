#pragma once
#include <QtGui/QColor>
#include <algorithm>

/**
 * @brief 将标量值映射为颜色（简单科学渐变：深蓝->青->黄->红）
 * @param x 归一化值 [0,1]
 */
inline QColor colorMap(float x)
{
  x = std::clamp(x, 0.0f, 1.0f);

  // 分段线性：0:蓝 0.33:青 0.66:黄 1:红
  auto lerp = [](float a, float b, float t)
  { return a + (b - a) * t; };

  float r = 0, g = 0, b = 0;

  if (x < 0.33f)
  {
    float t = x / 0.33f;
    // 蓝(0,0,140) -> 青(0,255,255)
    r = 0;
    g = lerp(0, 255, t);
    b = lerp(140, 255, t);
  }
  else if (x < 0.66f)
  {
    float t = (x - 0.33f) / 0.33f;
    // 青(0,255,255) -> 黄(255,255,0)
    r = lerp(0, 255, t);
    g = 255;
    b = lerp(255, 0, t);
  }
  else
  {
    float t = (x - 0.66f) / 0.34f;
    // 黄(255,255,0) -> 红(255,0,0)
    r = 255;
    g = lerp(255, 0, t);
    b = 0;
  }
  return QColor((int)r, (int)g, (int)b);
}
