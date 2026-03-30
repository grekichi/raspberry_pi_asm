#ifndef __GY91_H__
#define __GY91_H__


#define AK8963_WAI              0x00  // Device ID
#define AK8963_REG_ST1          0x02  // Status 1
#define AK8963_REG_HXL          0x03
#define AK8963_REG_HXH          0x04
#define AK8963_REG_HYL          0x05
#define AK8963_REG_HYH          0x06
#define AK8963_REG_HZL          0x07
#define AK8963_REG_HZL          0x08
#define AK8963_REG_ST2          0x09  // Status 2
#define AK8963_REG_CNTL1        0x0A  // Control 1
#define AK8963_REG_CNTL2        0x0B  // Contro
#define AK8963_I2C_ADDRESS      0x0C
#define AK8963_REG_ASAX         0x10  // X-axis sensitivity adjustment value
#define AK8963_REG_ASAY         0x11  // Y-axis sensitivity adjustment value
#define AK8963_REG_ASAZ         0x12  // Z-axis sensitivity adjustment value

#define AK8963_RESP_WAI         0x48


//  --- BMP280補足 <I2C Interface> ---
//  7ビットのデバイスアドレスは 111011x で、上位6ビットは固定です。
//  最後のビットは SDO 値によって変更可能であり、動作中に変更できます
//  SDOをGNDに接続するとスレーブアドレスは 1110110（0x76）になり、
//  VDDIO に接続するとスレーブアドレスは 1110111（0x77）になります。
#define BMP280_I2C_ADDRESS      0x76  // MPU9250とのI2C通信用

#define BMP280_REG_CHIPID       0xD0
#define BMP280_REG_VERSION      0xD1
#define BMP280_REG_RESET        0xE0

#define BMP280_REG_STATUS       0xF3
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_PRESS_LSB    0xF8
#define BMP280_REG_PRESS_XLSB   0xF9
#define BMP280_REG_TEMP_MSB     0xFA
#define BMP280_REG_TEMP_LSB     0xFB
#define BMP280_REG_TEMP_XLSB    0xFC


#define MPU9250_SMPLRT_DIV      0x19

#define MPU9250_CONFIG          0x1A
#define MPU9250_GYRO_CONFIG     0x1B
#define MPU9250_ACCEL_CONFIG    0x1C
#define MPU9250_ACCEL_CONFIG_2  0x1D

#define MPU9250_I2C_MST_CTRL    0x24

#define MPU9250_I2C_SLV0_ADDR   0x25
#define MPU9250_I2C_SLV0_REG    0x26
#define MPU9250_I2C_SLV0_CTRL   0x27
#define MPU9250_I2C_SLV1_ADDR   0x28
#define MPU9250_I2C_SLV1_REG    0x29
#define MPU9250_I2C_SLV1_CTRL   0x2A
#define MPU9250_I2C_SLV2_ADDR   0x2B
#define MPU9250_I2C_SLV2_REG    0x2C
#define MPU9250_I2C_SLV2_CTRL   0x2D
#define MPU9250_I2C_SLV3_ADDR   0x2E
#define MPU9250_I2C_SLV3_REG    0x2F
#define MPU9250_I2C_SLV3_CTRL   0x30
#define MPU9250_I2C_SLV4_ADDR   0x31
#define MPU9250_I2C_SLV4_REG    0x32
#define MPU9250_I2C_SLV4_DO     0x33
#define MPU9250_I2C_SLV4_CTRL   0x34
#define MPU9250_I2C_SLV4_DI     0x35
#define MPU9250_I2C_MST_STATUS  0x36

#define MPU9250_INT_PIN_CFG     0x37
#define MPU9250_INT_ENABLE      0x38
#define MPU9250_INT_STATUS      0x3A

#define MPU9250_ACCEL_XOUT_H    0x3B
#define MPU9250_ACCEL_XOUT_L    0x3C
#define MPU9250_ACCEL_YOUT_H    0x3D
#define MPU9250_ACCEL_YOUT_L    0x3E
#define MPU9250_ACCEL_ZOUT_H    0x3F
#define MPU9250_ACCEL_ZOUT_L    0x40

#define MPU9250_TEMP_OUT_H      0x41
#define MPU9250_TEMP_OUT_L      0x42

#define MPU9250_GYRO_XOUT_H     0x43
#define MPU9250_GYRO_XOUT_L     0x44
#define MPU9250_GYRO_YOUT_H     0x45
#define MPU9250_GYRO_YOUT_L     0x46
#define MPU9250_GYRO_ZOUT_H     0x47
#define MPU9250_GYRO_ZOUT_L     0x48

#define MPU9250_EXT_SENS_DATA_00    0x49
#define MPU9250_EXT_SENS_DATA_01    0x4A
#define MPU9250_EXT_SENS_DATA_02    0x4B
#define MPU9250_EXT_SENS_DATA_03    0x4C
#define MPU9250_EXT_SENS_DATA_04    0x4D
#define MPU9250_EXT_SENS_DATA_05    0x4E
#define MPU9250_EXT_SENS_DATA_06    0x4F
#define MPU9250_EXT_SENS_DATA_07    0x50
#define MPU9250_EXT_SENS_DATA_08    0x51
#define MPU9250_EXT_SENS_DATA_09    0x52
#define MPU9250_EXT_SENS_DATA_10    0x53
#define MPU9250_EXT_SENS_DATA_11    0x54
#define MPU9250_EXT_SENS_DATA_12    0x55
#define MPU9250_EXT_SENS_DATA_13    0x56
#define MPU9250_EXT_SENS_DATA_14    0x57
#define MPU9250_EXT_SENS_DATA_15    0x58
#define MPU9250_EXT_SENS_DATA_16    0x59
#define MPU9250_EXT_SENS_DATA_17    0x5A
#define MPU9250_EXT_SENS_DATA_18    0x5B
#define MPU9250_EXT_SENS_DATA_19    0x5C
#define MPU9250_EXT_SENS_DATA_20    0x5D
#define MPU9250_EXT_SENS_DATA_21    0x5E
#define MPU9250_EXT_SENS_DATA_22    0x5F
#define MPU9250_EXT_SENS_DATA_23    0x60


#define MPU9250_I2C_SLV0_DO         0x63  // R/W
#define MPU9250_I2C_SLV1_DO         0x64  // R/W
#define MPU9250_I2C_SLV2_DO         0x65  // R/W
#define MPU9250_I2C_SLV3_DO         0x66  // R/W
#define MPU9250_I2C_MST_DELAY_CTRL  0x67  // R/W

#define MPU9250_I2C_ADDRESS     0x68
#define MPU9250_USER_CTRL       0x6A
#define MPU9250_PWR_MGMT_1      0x6B
#define MPU9250_PWR_MGMT_2      0x6C

#define MPU9250_RESP_WAI        0x71
#define MPU9250_WHO_AM_I        0x75


#endif