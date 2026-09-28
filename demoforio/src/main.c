#include <stdio.h>
#include "board.h"
#include "ecat_def.h"
#include "ecatappl.h"
#include "ecatslv.h"
#include "applInterface.h"
#include "digital_io.h"
#include "hpm_ecat_hw.h"
#include "hpm_gpio_drv.h"

int main(void)
{
    board_init();
    board_init_ethercat(HPM_ESC);   /* ESC 引脚 + PHY 复位 */
    board_init_led_pins();          /* 现成函数：RGB 三灯设为 GPIO 输出 */

    /* 初始化成功指示：点亮 RGB 三灯（红/绿/蓝），确认板级 GPIO 正常 */
    gpio_write_pin(BOARD_R_GPIO_CTRL, BOARD_R_GPIO_INDEX, BOARD_R_GPIO_PIN, BOARD_LED_ON_LEVEL); /* PB25 红 */
    board_delay_ms(300);
    gpio_write_pin(BOARD_G_GPIO_CTRL, BOARD_G_GPIO_INDEX, BOARD_G_GPIO_PIN, BOARD_LED_ON_LEVEL); /* PB24 绿 */
    board_delay_ms(300);
    gpio_write_pin(BOARD_B_GPIO_CTRL, BOARD_B_GPIO_INDEX, BOARD_B_GPIO_PIN, BOARD_LED_ON_LEVEL); /* PA09 蓝 */
    board_delay_ms(300);

    printf("EtherCAT IO sample\r\n");

    if (ecat_hardware_init(HPM_ESC) != status_success) {
        printf("Init ESC failed!\r\n");
        return 0;
    }

    MainInit();                     /* SSC 协议栈初始化 */

#if defined(ESC_EEPROM_EMULATION) && ESC_EEPROM_EMULATION
    pAPPL_EEPROM_Read   = ecat_eeprom_emulation_read;
    pAPPL_EEPROM_Write  = ecat_eeprom_emulation_write;
    pAPPL_EEPROM_Reload = ecat_eeprom_emulation_reload;
    pAPPL_EEPROM_Store  = ecat_eeprom_emulation_store;
#endif

    APPL_GenerateMapping(&nPdInputSize, &nPdOutputSize);
    bRunApplication = TRUE;

    while (bRunApplication == TRUE) {
        MainLoop();
    }

    return 0;
}
