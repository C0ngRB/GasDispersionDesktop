# GasDispersionDesktop (Qt6)

一个桌面端交互平台：设置风向/风速/点源/泄漏强度/扩散参数，点击运行后动态可视化气体扩散态势。
数值内核为 2D 对流-扩散方程（被动标量）显式有限差分：迎风对流 + 中心差分扩散。

## 依赖
- Qt 6.x (Widgets)
- CMake >= 3.20
- C++17 编译器（MSVC/Clang/GCC）

## 构建（Windows / Visual Studio）
```powershell
# 1) 配置（生成 build-mingw 构建树）
cmake -S . -B build-mingw -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.9.3\mingw_64"

# 2) 构建（生成 exe）
cmake --build build-mingw -- -j 8

