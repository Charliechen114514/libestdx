// 01_register_led:裸寄存器点灯 —— 不走 HAL_GPIO_*。
// RCC 开 GPIOC 时钟,CRH 把 PC13 配成推挽输出 2MHz,BSRR/BRR 直接写位;
// HAL_Delay 仍用(延迟不是本篇的变量)。03_led 的对照篇:同一盏灯,
// 这边每一笔都落在寄存器地址上,那边全部交给官方库。
#include <cstdint>

#include "stm32f1xx_hal.h"

static void SystemClock_Config() {
    RCC_OscInitTypeDef osc{};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    osc.PLL.PLLMUL = RCC_PLL_MUL16;
    HAL_RCC_OscConfig(&osc);

    RCC_ClkInitTypeDef clk{};
    clk.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

int main() {
    HAL_Init();
    SystemClock_Config();

    // 1) GPIOC 时钟:APB2ENR bit4(IOPENR)。不开钟,寄存器写了也白写。
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    // 2) PC13 模式:pin ≥ 8 的配置段住在 CRH(每脚 4 位:pin13 → 偏移
    //    (13-8)*4=20,CNF 占 bit23:22,MODE 占 bit21:20)。第一版把这段写进
    //    CRL(配成了 pin5),PC13 保持复位默认的浮空输入,BSRR/BRR 写了
    //    也不驱动引脚 —— Renode 里 ODR 读数纹丝不动,当场抓包。
    //    清 CNF=00(推挽)、MODE=10(2MHz 输出)。
    GPIOC->CRH &= ~(0xFu << 20);
    GPIOC->CRH |= (0x2u << 20);

    for (;;) {
        GPIOC->BSRR = 1u << 13;  // 置位 = ODR13=1 = LED 灭(ActiveLow)
        HAL_Delay(500);
        GPIOC->BRR = 1u << 13;   // 复位 = ODR13=0 = LED 亮
        HAL_Delay(500);
    }
}
