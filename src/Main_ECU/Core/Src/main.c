/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "string.h"
#include "stdio.h"
#include "ILI9225.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Định nghĩa CAN ID
#define MAIN_TO_LIGHT_ID      0x101
#define PARKING_TO_MAIN_ID    0x202
#define LIGHT_TO_MAIN_ID      0x303
#define AIRBAG_TO_MAIN_ID     0x404

// Lệnh gửi cho ECU Đèn
#define CMD_TOGGLE_LED   0x01
#define CMD_SWITCH_MODE  0x02

// Nút nhấn
#define BUTTON_PIN GPIO_PIN_0
#define BUTTON_PORT GPIOA
#define SHORT_PRESS_TIME 50
#define LONG_PRESS_TIME  1000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
// Biến cho giao tiếp CAN
CAN_TxHeaderTypeDef TxHeader;
CAN_RxHeaderTypeDef RxHeader;
uint8_t TxData[8];
uint8_t RxData[8];
uint32_t TxMailbox;

// Biến xử lý nút nhấn
uint8_t lastButtonState = GPIO_PIN_SET;
uint32_t buttonPressTime = 0;
volatile uint8_t crash_occured = 0; // Cờ báo va chạm, =1 thì dừng mọi thứ

// Biến lưu trữ trạng thái từ các ECU khác
char parking_status_str[15] = "WAITING...";
char last_displayed_parking_status[15] = "";
volatile uint8_t light_state = 0; // 0=OFF, 1=ON
volatile uint8_t light_mode = 0;  // 0=AUTO, 1=MANUAL
volatile uint8_t newParkingData = 1; // Cờ báo có dữ liệu mới từ cảm biến lùi, 1 để vẽ lần đầu
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
static void CAN_Filter_Config(void);
void handle_button_presses(void);
void update_lcd_parking_status(void);
void draw_initial_screen(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_CAN_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  // Khởi tạo màn hình LCD
  // Khởi tạo màn hình LCD
  lcd_init();

  // Cấu hình bộ lọc và khởi động CAN
  CAN_Filter_Config();
  if (HAL_CAN_Start(&hcan) != HAL_OK) Error_Handler();
  if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) Error_Handler();

  // Vẽ giao diện ban đầu
  draw_initial_screen();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	    // Nếu chưa có va chạm, hệ thống hoạt động bình thường
	    if (!crash_occured)
	    {
	      // 1. Kiểm tra và xử lý nút nhấn để gửi lệnh đi
	      handle_button_presses();

	      // 2. Cập nhật màn hình LCD nếu có dữ liệu mới từ cảm biến
	      update_lcd_parking_status();

	      // Bạn có thể thêm code cập nhật trạng thái đèn/chế độ đèn lên LCD ở đây nếu muốn
	    }

	    // Nếu có va chạm, vòng lặp sẽ không làm gì cả, chỉ chờ reset.
	    // Màn hình đã được xử lý trong ngắt.

	    HAL_Delay(20); // Delay nhỏ để giảm tải CPU
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

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 4;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_12TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_5TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = ENABLE;
  hcan.Init.TransmitFifoPriority = ENABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA3 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
 * @brief Vẽ giao diện ban đầu lên LCD
 */
void draw_initial_screen(void)
{
    fill_rectangle(0, 0, 220, 176, COLOR_NAVY);
    draw_string(10, 20, COLOR_WHITE, 1.5, "Parking Status:");
    draw_string(10, 100, COLOR_WHITE, 1.5, "Light Control:");
    // Thêm các thông tin khác nếu cần
}

/**
  * @brief Cấu hình bộ lọc CAN để nhận tin từ 3 ECU: Cảm biến, Đèn, Túi khí.
  */
static void CAN_Filter_Config(void)
{
    CAN_FilterTypeDef sFilterConfig;

    // --- Filter Bank 0: Lọc tin từ ECU Cảm biến lùi (0x202) và ECU Đèn (0x303) ---
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDLIST; // Lọc theo danh sách ID
    sFilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;
    sFilterConfig.FilterIdHigh = PARKING_TO_MAIN_ID << 5; // ID 0x202
    sFilterConfig.FilterIdLow = LIGHT_TO_MAIN_ID << 5;    // ID 0x303
    sFilterConfig.FilterMaskIdHigh = 0xFFFF; // Cả hai ID phải khớp chính xác
    sFilterConfig.FilterMaskIdLow = 0xFFFF;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    // --- Filter Bank 1: Lọc tin từ ECU Túi khí (0x404) ---
    sFilterConfig.FilterBank = 1;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDLIST;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_16BIT;
    sFilterConfig.FilterIdHigh = AIRBAG_TO_MAIN_ID << 5; // Chỉ lọc ID 0x404
    sFilterConfig.FilterIdLow = 0;
    sFilterConfig.FilterMaskIdHigh = 0xFFFF;
    sFilterConfig.FilterMaskIdLow = 0;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
  * @brief Hàm callback xử lý sự kiện khi có tin nhắn CAN mới đến.
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
    {
        // ƯU TIÊN SỐ 1: Kiểm tra tin nhắn va chạm từ ECU Túi Khí
        if (RxHeader.StdId == AIRBAG_TO_MAIN_ID)
        {
            if (strncmp((char*)RxData, "CRASH", 5) == 0)
            {
                crash_occured = 1; // Đặt cờ báo va chạm

                // Hiển thị cảnh báo đỏ toàn màn hình
                fill_rectangle(0, 0, 220, 176, COLOR_RED);
                draw_string(20, 50, COLOR_WHITE, 2.0, "  CRASH   ");
                draw_string(20, 80, COLOR_WHITE, 2.0, " DETECTED!");

                HAL_CAN_DeactivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
            }
        }
        // Kiểm tra tin nhắn từ ECU CẢM BIẾN
        else if (RxHeader.StdId == PARKING_TO_MAIN_ID)
        {
            if (!crash_occured) // Chỉ xử lý nếu chưa có va chạm
            {
                memcpy(parking_status_str, RxData, RxHeader.DLC);
                parking_status_str[RxHeader.DLC] = '\0';
                newParkingData = 1;
            }
        }
        // Kiểm tra tin nhắn từ ECU ĐÈN
        else if (RxHeader.StdId == LIGHT_TO_MAIN_ID)
        {
            if (!crash_occured && RxHeader.DLC == 1)
            {
                light_state = RxData[0] & 0x01;
                light_mode = (RxData[0] >> 1) & 0x01;
            }
        }
    }
}


/**
  * @brief Hàm kiểm tra nút nhấn và gửi lệnh qua CAN bus tới ECU Đèn
  */
void handle_button_presses(void)
{
    uint8_t currentButtonState = HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);

    if (currentButtonState == GPIO_PIN_RESET && lastButtonState == GPIO_PIN_SET)
    {
      buttonPressTime = HAL_GetTick();
    }
    else if (currentButtonState == GPIO_PIN_SET && lastButtonState == GPIO_PIN_RESET)
    {
      uint32_t pressDuration = HAL_GetTick() - buttonPressTime;

      TxHeader.StdId = MAIN_TO_LIGHT_ID;
      TxHeader.IDE = CAN_ID_STD;
      TxHeader.RTR = CAN_RTR_DATA;
      TxHeader.DLC = 1;

      if (pressDuration > LONG_PRESS_TIME)
      {
        TxData[0] = CMD_SWITCH_MODE;
        HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
      }
      else if (pressDuration > SHORT_PRESS_TIME)
      {
        TxData[0] = CMD_TOGGLE_LED;
        HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
      }
    }
    lastButtonState = currentButtonState;
}

/**
  * @brief Hàm cập nhật màn hình LCD dựa trên trạng thái cảm biến lùi
  */
void update_lcd_parking_status(void)
{
    if (newParkingData) {
      newParkingData = 0;

      if (strcmp(parking_status_str, last_displayed_parking_status) != 0)
      {
          fill_rectangle(40, 50, 220, 176, COLOR_NAVY); // Xóa vùng trạng thái cũ

          uint16_t color = COLOR_GREEN;
          if (strcmp(parking_status_str, "SLOW") == 0) {
              color = COLOR_ORANGE;
          } else if (strcmp(parking_status_str, "STOP") == 0) {
              color = COLOR_RED;
          } else if (strcmp(parking_status_str, "SAFE") == 0) {
              color = COLOR_GREEN;
          } else { // "WAITING..."
              color = COLOR_WHITE;
          }

          draw_string(40, 50, color, 2.0, parking_status_str);
          strcpy(last_displayed_parking_status, parking_status_str);
      }
    }
}
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

#ifdef  USE_FULL_ASSERT
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
