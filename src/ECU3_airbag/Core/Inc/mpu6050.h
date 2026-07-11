#ifndef MPU6050_H
#define MPU6050_H

#include "main.h"

// Địa chỉ I2C của MPU6050
#define MPU6050_ADDR 0x68 << 1 // Địa chỉ 7-bit, shift trái 1 bit cho HAL

// Thanh ghi MPU6050
#define MPU6050_WHO_AM_I 0x75
#define MPU6050_PWR_MGMT_1 0x6B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_SMPLRT_DIV 0x19

// Hàm khởi tạo và đọc dữ liệu
HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, int16_t *accel_data);
HAL_StatusTypeDef MPU6050_Calibrate(I2C_HandleTypeDef *hi2c, int16_t *accel_offset);

#endif // MPU6050_H
