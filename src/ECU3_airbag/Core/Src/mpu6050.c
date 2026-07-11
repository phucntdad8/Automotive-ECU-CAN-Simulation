#include "mpu6050.h"

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t check, data;
    HAL_StatusTypeDef status;

    // Kiểm tra kết nối MPU6050 qua WHO_AM_I
    status = HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_WHO_AM_I, 1, &check, 1, 100);
    if (status != HAL_OK || check != 0x68) {
        // Thử địa chỉ 0x69 nếu 0x68 thất bại (AD0 = VCC)
        status = HAL_I2C_Mem_Read(hi2c, (0x69 << 1), MPU6050_WHO_AM_I, 1, &check, 1, 100);
        if (status != HAL_OK || check != 0x68) {
            return HAL_ERROR; // Không tìm thấy MPU6050
        }
    }

    // Thoát chế độ sleep
    data = 0x00;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_PWR_MGMT_1, 1, &data, 1, 100);
    if (status != HAL_OK) return status;

    // Cấu hình tần số lấy mẫu (8kHz / (1 + 7) = 1kHz)
    data = 0x07;
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_SMPLRT_DIV, 1, &data, 1, 100);
    if (status != HAL_OK) return status;

    // Cấu hình thang đo gia tốc ±2g
    data = 0x00; // 00: ±2g
    status = HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_ACCEL_CONFIG, 1, &data, 1, 100);
    if (status != HAL_OK) return status;

    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, int16_t *accel_data) {
    uint8_t buffer[6];
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, 1, buffer, 6, 100);
    if (status != HAL_OK) return status;

    accel_data[0] = (int16_t)(buffer[0] << 8 | buffer[1]); // Accel X
    accel_data[1] = (int16_t)(buffer[2] << 8 | buffer[3]); // Accel Y
    accel_data[2] = (int16_t)(buffer[4] << 8 | buffer[5]); // Accel Z
    return HAL_OK;
}

HAL_StatusTypeDef MPU6050_Calibrate(I2C_HandleTypeDef *hi2c, int16_t *accel_offset) {
    int16_t accel_data[3];
    int32_t sum[3] = {0};
    const int samples = 100;

    for (int i = 0; i < samples; i++) {
        if (MPU6050_Read_Accel(hi2c, accel_data) != HAL_OK) return HAL_ERROR;
        sum[0] += accel_data[0];
        sum[1] += accel_data[1];
        sum[2] += accel_data[2];
        HAL_Delay(10);
    }

    accel_offset[0] = sum[0] / samples;
    accel_offset[1] = sum[1] / samples;
    accel_offset[2] = sum[2] / samples - 16384; // Trừ 1g trên trục Z
    return HAL_OK;
}
