.. _spi_lcd_motor_foc_controller:

SPI LCD + BLDC FOC 触摸控制例程
================================

概述
----

本例程把 SPI LCD、XPT2046 触摸、LVGL 和 ``motor_ctrl/bldc_foc`` 电机控制合并为
一个独立工程。原始 ``samples/motor_ctrl/bldc_foc`` 不参与本工程编译，也不需要单独
烧录。

界面功能：

* 圆形控件设置目标转速，范围为 0～2400 RPM；
* ``START`` 启动电机；
* ``STOP`` 立即关闭控制环和 PWM 输出；
* ``SPEED +`` / ``SPEED -`` 每次增减 100 RPM，长按可连续调节；
* 显示目标转速、实际转速和 READY/RUNNING/STOPPED/FAULT 状态。

工程结构
--------

* ``src/main.c``：系统入口，连接 LCD 和电机控制模块；
* ``src/lcd/``：LCD、触摸、LVGL 适配层、字库及电机控制界面；
* ``src/lcd/lcd_app.c``：LCD/触摸硬件、LVGL 显示与输入设备初始化；
* ``src/lcd/motor_control_ui.c``：LVGL 电机控制界面；
* ``src/motor_foc/bldc_foc.c``：本工程私有的 FOC 实现副本；
* ``src/motor_foc/motor_foc_control.h``：界面调用的电机控制 API；
* ``src/motor_foc/mcl_app_config.h``：MCL 配置。

调用关系
--------

程序只有一个入口 ``src/main.c::main()``。它调用
``motor_foc_control_init()`` 完成 FOC 初始化和转子对齐，然后进入 LVGL 循环。
界面按钮通过 ``motor_foc_control.h`` 调用本地 FOC 副本。因此 LCD 和 FOC 不是两个
相互烧录的固件，而是同一个固件中的两个模块。

硬件注意事项
------------

* 当前构建目标为 ``hpm6e00YZ``；
* 电机、驱动板、电流采样和编码器接线必须与原 ``bldc_foc`` 例程一致；
* LCD 使用 SPI1，触摸使用 SPI3，电机 PWM 使用 PWM0；
* 首次上电前请架空电机或降低母线电压，并确认过流保护、相序和编码器方向；
* 如果触摸坐标偏移或方向错误，修改 ``src/lcd/main.h`` 中的 ``TOUCH_*`` 校准宏。

构建
----

在 SDK 根目录执行：

.. code-block:: console

   cmake -S samples/spi_components/full_duplex/polling/master_lcd -B samples/spi_components/full_duplex/polling/master_lcd/build_motor_ui -G Ninja -DBOARD=hpm6e00YZ -DHPM_BUILD_TYPE=flash_xip -DCMAKE_BUILD_TYPE=Debug
   cmake --build samples/spi_components/full_duplex/polling/master_lcd/build_motor_ui --parallel 8

烧录
----

只烧录本工程生成的一个固件：

``build_motor_ui/output/demo.elf``

如果所用烧录工具只接受二进制文件，则选择：

``build_motor_ui/output/demo.bin``

下载方式和地址沿用 HPM SDK 的 ``hpm6e00YZ / flash_xip`` 配置。不要再烧录原
``samples/motor_ctrl/bldc_foc`` 生成的固件，否则它会覆盖 LCD 控制固件。

运行过程
--------

上电后先初始化 LCD 和触摸，再进行电流零点采样与转子对齐。界面显示 ``READY`` 后，
先设置较低的目标转速，再按 ``START``。按 ``STOP`` 会关闭控制环和 PWM 输出。

串口会输出：

.. code-block:: console

   SPI touch-screen BLDC FOC controller
   Initializing motor FOC and aligning rotor...
   Motor FOC: ready
