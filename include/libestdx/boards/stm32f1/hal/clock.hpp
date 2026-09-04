#pragma once
// 时钟源基准。BluePill 板载 8MHz 晶振;换板子改 HSE_VALUE 即可。

#define HSE_VALUE             8000000U /* 外部高速晶振 Hz */
#define HSE_STARTUP_TIMEOUT      100U  /* HSE 启动超时 ms */
#define HSI_VALUE             8000000U /* 内部 RC */
#define LSI_VALUE              40000U
#define LSE_VALUE              32768U
#define LSE_STARTUP_TIMEOUT     5000U
#define VDD_VALUE               3300U  /* mV */
