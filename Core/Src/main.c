/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  BTN_IDLE,     // 空闲，等待按下
  BTN_PRESSING, // 抖动/等待稳定按下
  BTN_PRESSED,  // 稳定按下，等待松开
  BTN_RELEASING // 抖动/等待稳定松开
} BTN_StateTypeDef;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define LOW GPIO_PIN_RESET
#define HIGH GPIO_PIN_SET

#define BTN_PIN GPIO_PIN_7
#define BTN_PORT GPIOA

#define LED_PIN GPIO_PIN_13
#define LED_PORT GPIOC

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
const uint16_t DEBOUNCE_MS = 20;
uint16_t btn_cnt = 0;
uint8_t btn_state = 0;
volatile BTN_StateTypeDef btn_debounce_flag = BTN_IDLE;
uint32_t btn_pressed_time = 0;
/*
0: interval = 250ms
1: interval = 500ms
2: interval = 1000ms
*/
uint8_t blink_type = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void btn_debounce();
void led_blink(int);

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim4)
  { /* 1ms trigger once */
    btn_debounce();
  }
  else if (htim == &htim3)
  { /* 250ms trigger once */
    led_blink(blink_type);
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BTN_PIN && btn_debounce_flag == BTN_IDLE)
  {
    // 在按键中断里面去抖
    btn_pressed_time = HAL_GetTick(); // 重置按键按下时间
    btn_debounce_flag = BTN_PRESSING; // 设置按键状态为按下,但是未去抖
  }
}

void led_blink(int type)
{
  static uint8_t blink_state = 0; /* 这里用static是为了避免在全局作用域拉石 */

  /*
  这里用了个取巧的方式
  我们已知这个函数每250ms触发一次
  那么对于目标的 250/500/1000ms 的GPIO电平转换间隔, 我们可以写出如下的计数器映射表
  250  -> 1 = 2^0
  500  -> 2 = 2^1
  1000 -> 4 = 2^2

  只要我们的counter循环计数的基础上每 250ms +1
  那么在每次 counter=0 的时候转换电平就能切换间隔时间
  */
  blink_state++;
  blink_state %= (uint8_t)pow(2, type); // 这个如果是大数据的话其实推荐预计算(

  if (blink_state == 0)
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}

void btn_debounce()
{
  // 1ms一次的去抖
  if (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == HIGH)
  {
    if (btn_cnt < 0xFFFF) // 防止溢出 (应该没有人会按着超过1分钟吧
      btn_cnt++;          // 1ms计数一次
  }
  else
  {
    if (btn_cnt >= DEBOUNCE_MS) // 如果按的足够久就置flag, 并在主循环函数里面处理
      btn_state = 1;
    btn_cnt = 0; // 松开清空
  }
}

void btn_debounce_BTN_EXTI()
{
  GPIO_PinState STATE = HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN);
  uint32_t current_time = HAL_GetTick();
  switch (btn_debounce_flag)
  {
  case BTN_IDLE:
    break;
  case BTN_PRESSING:
    if (current_time - btn_pressed_time >= DEBOUNCE_MS && STATE == HIGH)
    {
      btn_debounce_flag = BTN_PRESSED;
    }
    else if (STATE == LOW)
    {
      btn_debounce_flag = BTN_IDLE; // 还没到阈值就松开了, 回到空闲状态
    }
    break;
  case BTN_PRESSED:
    if (STATE == LOW)
    {
      btn_debounce_flag = BTN_RELEASING;
      btn_pressed_time = current_time; // 重置按键按下时间
    }
    break;
  case BTN_RELEASING:
    if (current_time - btn_pressed_time >= DEBOUNCE_MS && STATE == LOW)
    {
      btn_debounce_flag = BTN_IDLE;
      btn_state = 1; // 按键按下事件
    }
    else if (STATE == HIGH)
    {
      btn_debounce_flag = BTN_PRESSED; // 还没到阈值就按下了, 回到按下状态
    }
    break;
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  // HAL_TIM_Base_Start_IT(&htim4);
  HAL_TIM_Base_Start_IT(&htim3);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    btn_debounce_BTN_EXTI();
    if (btn_state == 1)
    {
      blink_type++;
      blink_type %= 3;
      btn_state = 0;
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
