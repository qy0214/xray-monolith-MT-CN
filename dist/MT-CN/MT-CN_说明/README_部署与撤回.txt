MT-CN 部署 / 撤回 说明（DLSS·FSR3 版）
========================================

一、部署（装我们的引擎 + DLSS/FSR3）
1. 打开 MODS\JSGME.exe
2. 左侧选中 MT-CN → 点 “>”（启用）
3. 在游戏目录双击 清空着色器缓存.bat（JSGME 不会替你清缓存，装/撤都必须清）
4. 启动游戏

二、撤回（回到原版引擎）
1. JSGME → 左侧选中 MT-CN → 点 “<”（停用）
   这一步会把 bin\ / db\ / gamedata\ 全部还原成整合包原有文件（包括原版 exe、原版 nvngx_dlss.dll、原版着色器）
2. 再在游戏目录双击 清空着色器缓存.bat
3. 启动游戏

三、注意
- 本 mod 里的着色器覆盖（gamedata\shaders\r3\*）
- 每次换引擎版本（换 bin\AnomalyDX11AVX.exe）后都要清一次着色器缓存。
- 校验清单：MODS\MT-CN.sha256.txt
