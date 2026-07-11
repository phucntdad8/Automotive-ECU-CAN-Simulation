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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Định nghĩa các CAN ID
#define MAIN_TO_LIGHT_ID 0x101 // ID dùng để nhận lệnh từ Main ECU
#define LIGHT_TO_MAIN_ID 0x303 // ID dùng để gửi trạng thái về Main ECU

// Định nghĩa phần cứng
#define LED_PIN       GPIO_PIN_0
#define LED_PORT      GPIOA
#define BH1750_ADDR   (0x23 << 1) // Địa chỉ I2C của BH1750
#define LIGHT_THRESHOLD 100 // Ngưỡng ánh sáng (lux) để bật/tắt đèn

// Định nghĩa các lệnh nhận được
#define CMD_TOGGLE_LED   0x01
#define CMD_SWITCH_MODE  0x02

// Định nghĩa các chế độ hoạt động
#define MODE_AUTO   0
#define MODE_MANUAL 1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan;

I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
// Biến cho CAN
CAN_TxHeaderTypeDef TxHeader;
CAN_RxHeaderTypeDef RxHeader;
uint8_t TxData[8];
uint8_t RxData[8];
uint32_t TxMailbox;
volatile uint8_t canRxFlag = 0; // Cờ báo có tin nhắn đến

// Biến trạng thái của hệ thống đèn
uint8_t controlMode = MODE_AUTO; // Mặc định là chế độ tự động
uint8_t ledState = 0;           // Trạng thái đèn: 0: OFF, 1: ON
uint8_t lastSentCombinedState = 0xFF; // Giá trị khởi tạo để đảm bảo gửi lần đầu

float lux_value = 0.0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
static void CAN_Filter_Config(void);
HAL_StatusTypeDef BH1750_Init(void);
HAL_StatusTypeDef BH1750_ReadLux(float *lux);
void send_status_update_to_main_ecu(void);
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
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  BH1750_Init(); // Khởi tạo cảm biến ánh sáng

  // Cấu hình bộ lọc và khởi động CAN
  CAN_Filter_Config();
  if (HAL_CAN_Start(&hcan) != HAL_OK) Error_Handler();
  if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) Error_Handler();

  // Cấu hình CAN Header để gửi phản hồi về Main ECU
  TxHeader.StdId = LIGHT_TO_MAIN_ID;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 1; // Luôn gửi 1 byte trạng thái
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	    // 1. Xử lý lệnh nhận được từ Main ECU (nếu có)
	    if (canRxFlag) {
	        canRxFlag = 0; // Xóa cờ ngay
	        // Kiểm tra đúng ID và độ dài dữ liệu
	        if (RxHeader.StdId == MAIN_TO_LIGHT_ID && RxHeader.DLC == 1) {
	            uint8_t command = RxData[0];
	            if (command == CMD_SWITCH_MODE) {
	                controlMode = !controlMode; // Đảo chế độ
	            } else if (command == CMD_TOGGLE_LED) {
	                if (controlMode == MODE_MANUAL) { // Chỉ cho phép đảo LED ở chế độ thủ công
	                    ledState = !ledState;
	                }
	            }
	        }
	    }

	    // 2. Logic điều khiển đèn dựa trên chế độ hiện tại
	    if (controlMode == MODE_AUTO) {
	      if (BH1750_ReadLux(&lux_value) == HAL_OK) {
	        ledState = (lux_value < LIGHT_THRESHOLD) ? 1 : 0; // 1 = ON, 0 = OFF
	      }
	    }
	    // Ở chế độ MANUAL, ledState chỉ thay đổi bởi lệnh từ CAN bus

	    // 3. Cập nhật trạng thái vật lý của đèn LED
	    // Giả sử LED sáng ở mức cao (HIGH)
	    HAL_GPIO_WritePin(LED_PORT, LED_PIN, (ledState == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);

	    // 4. Gửi trạng thái (chế độ và đèn) về cho Main ECU nếu có sự thay đổi
	    send_status_update_to_main_ecu();

	    HAL_Delay(100); // Tần suất kiểm tra và cập nhật
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
 * @brief Cấu hình bộ lọc CAN để chỉ nhận các tin nhắn có ID = MAIN_TO_LIGHT_ID (0x101)
 */
static void CAN_Filter_Config(void)
{
    CAN_FilterTypeDef sFilterConfig;

    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    // Chỉ chấp nhận ID là 0x101
    sFilterConfig.FilterIdHigh = MAIN_TO_LIGHT_ID << 5;
    sFilterConfig.FilterIdLow = 0x0000;
    // Mask yêu cầu tất cả các bit của ID phải khớp
    sFilterConfig.FilterMaskIdHigh = 0x7FF << 5;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;

    if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Hàm Callback được gọi tự động khi có tin nhắn CAN trong FIFO0
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  // Đọc tin nhắn và đặt cờ báo
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
  {
      canRxFlag = 1;
  }
}

/**
 * @brief Gửi các lệnh khởi tạo cho cảm biến BH1750.
 */
HAL_StatusTypeDef BH1750_Init(void){
	uint8_t cmd_power_on = 0x01;
	uint8_t cmd_cont_hres = 0x10;
	if (HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR, &cmd_power_on, 1, 100) != HAL_OK) return HAL_ERROR;
	HAL_Delay(10);
	if (HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR, &cmd_cont_hres, 1, 100) != HAL_OK) return HAL_ERROR;
	return HAL_OK;
}

/**
 * @brief Đọc giá trị lux từ cảm biến BH1750.
 */
HAL_StatusTypeDef BH1750_ReadLux(float *lux) {
    uint8_t data[2];
    if (HAL_I2C_Master_Receive(&hi2c1, BH1750_ADDR, data, 2, HAL_MAX_DELAY) != HAL_OK) {
        *lux = -1.0; // Trả về giá trị lỗi
        return HAL_ERROR;
    }
    // Chuyển đổi 2 byte thành giá trị lux
    *lux = ((data[0] << 8) | data[1]) / 1.2;
    return HAL_OK;
}

/**
 * @brief Gửi trạng thái hiện tại (chế độ và đèn) về Main ECU.
 *        Hàm này mã hóa 2 thông tin vào 1 byte để tiết kiệm.
 */
void send_status_update_to_main_ecu(void) {
    // Mã hóa: bit 1 là chế độ (1=MANUAL, 0=AUTO), bit 0 là trạng thái đèn (1=ON, 0=OFF)
    uint8_t combinedState = (controlMode << 1) | ledState;

    // Chỉ gửi nếu trạng thái đã thay đổi so với lần gửi cuối
    if (combinedState != lastSentCombinedState) {
        TxData[0] = combinedState;

        // Chờ mailbox trống
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0) {
            if (HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox) == HAL_OK) {
                // Nếu gửi thành công, cập nhật trạng thái đã gửi
                lastSentCombinedState = combinedState;
            }
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
