#include "modbus_server.h"
#include "auto_tune.h"
#include "pid_controller.h"
#include <string.h>

static UART_HandleTypeDef huart_modbus;
static uint8_t rtu_rx_buf[MODBUS_RTU_FRAME_MAX];
static uint8_t rtu_tx_buf[MODBUS_RTU_FRAME_MAX];

static void Modbus_RTU_SendFrame(uint8_t *data, uint16_t len);

void Modbus_Init(void) {
    memset(g_sys.modbus_holding_regs, 0, sizeof(g_sys.modbus_holding_regs));

#if MODBUS_RTU_ENABLED
    __HAL_RCC_USART3_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLUP;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;
    gpio.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOC, &gpio);

    huart_modbus.Instance          = MODBUS_RTU_USART;
    huart_modbus.Init.BaudRate     = MODBUS_RTU_BAUDRATE;
    huart_modbus.Init.WordLength   = UART_WORDLENGTH_8B;
    huart_modbus.Init.StopBits     = UART_STOPBITS_1;
    huart_modbus.Init.Parity       = UART_PARITY_NONE;
    huart_modbus.Init.Mode         = UART_MODE_TX_RX;
    huart_modbus.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart_modbus.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart_modbus);
#endif
}

/* ==================== Modbus RTU ==================== */
static void Modbus_RTU_SendFrame(uint8_t *data, uint16_t len) {
    uint16_t crc = Modbus_CRC16(data, len - 2);
    data[len - 2] = crc & 0xFF;
    data[len - 1] = (crc >> 8) & 0xFF;
    HAL_UART_Transmit(&huart_modbus, data, len, 100);
}

void Modbus_RTU_Process(void) {
    HAL_StatusTypeDef status;
    uint16_t len = 0;

    /* Simple polling read — in production, use interrupt/DMA + idle line detection */
    status = HAL_UART_Receive(&huart_modbus, rtu_rx_buf, MODBUS_RTU_FRAME_MAX, 10);
    if (status != HAL_OK) return;

    /* Minimum Modbus RTU frame: DeviceAddr(1) + FC(1) + CRC(2) */
    if (rtu_rx_buf[0] != MODBUS_DEVICE_ID) return;

    uint8_t  fc       = rtu_rx_buf[1];
    uint16_t addr     = (rtu_rx_buf[2] << 8) | rtu_rx_buf[3];
    uint16_t quantity = (rtu_rx_buf[4] << 8) | rtu_rx_buf[5];
    uint16_t resp_len = 0;

    switch (fc) {
    case MODBUS_FC_READ_HOLDING: {
        if (addr + quantity > REG_COUNT) {
            Modbus_BuildException(fc, MODBUS_EXCEPTION_ILLEGAL_ADDR, rtu_tx_buf, &resp_len);
            Modbus_RTU_SendFrame(rtu_tx_buf, resp_len);
            return;
        }

        rtu_tx_buf[0] = MODBUS_DEVICE_ID;
        rtu_tx_buf[1] = MODBUS_FC_READ_HOLDING;
        rtu_tx_buf[2] = (uint8_t)(quantity * 2); /* Byte count */

        for (uint16_t i = 0; i < quantity; i++) {
            uint16_t val = Modbus_ReadHoldingReg(addr + i);
            rtu_tx_buf[3 + i * 2]       = (val >> 8) & 0xFF;
            rtu_tx_buf[3 + i * 2 + 1]   = val & 0xFF;
        }

        resp_len = 3 + quantity * 2 + 2; /* +2 for CRC */
        Modbus_RTU_SendFrame(rtu_tx_buf, resp_len);
        break;
    }

    case MODBUS_FC_WRITE_SINGLE: {
        if (addr >= REG_COUNT) {
            Modbus_BuildException(fc, MODBUS_EXCEPTION_ILLEGAL_ADDR, rtu_tx_buf, &resp_len);
            Modbus_RTU_SendFrame(rtu_tx_buf, resp_len);
            return;
        }

        uint16_t value = (rtu_rx_buf[4] << 8) | rtu_rx_buf[5];
        Modbus_WriteHoldingReg(addr, value);

        /* Echo response */
        memcpy(rtu_tx_buf, rtu_rx_buf, 6);
        resp_len = 8;
        Modbus_RTU_SendFrame(rtu_tx_buf, resp_len);
        break;
    }

    case MODBUS_FC_WRITE_MULTIPLE: {
        uint8_t byte_count = rtu_rx_buf[6];
        uint16_t reg_count = byte_count / 2;

        if (addr + reg_count > REG_COUNT) {
            Modbus_BuildException(fc, MODBUS_EXCEPTION_ILLEGAL_ADDR, rtu_tx_buf, &resp_len);
            Modbus_RTU_SendFrame(rtu_tx_buf, resp_len);
            return;
        }

        for (uint16_t i = 0; i < reg_count; i++) {
            uint16_t value = (rtu_rx_buf[7 + i * 2] << 8) | rtu_rx_buf[8 + i * 2];
            Modbus_WriteHoldingReg(addr + i, value);
        }

        /* Response: DeviceAddr + FC + StartAddr + Quantity */
        rtu_tx_buf[0] = MODBUS_DEVICE_ID;
        rtu_tx_buf[1] = MODBUS_FC_WRITE_MULTIPLE;
        rtu_tx_buf[2] = rtu_rx_buf[2];
        rtu_tx_buf[3] = rtu_rx_buf[3];
        rtu_tx_buf[4] = rtu_rx_buf[4];
        rtu_tx_buf[5] = rtu_rx_buf[5];
        resp_len = 8;
        Modbus_RTU_SendFrame(rtu_tx_buf, resp_len);
        break;
    }

    default:
        Modbus_BuildException(fc, MODBUS_EXCEPTION_ILLEGAL_FC, rtu_tx_buf, &resp_len);
        Modbus_RTU_SendFrame(rtu_tx_buf, resp_len);
        break;
    }
}

/* ==================== Modbus TCP ==================== */
void Modbus_TCP_Process(uint8_t *rx_data, uint16_t rx_len, uint8_t *tx_data, uint16_t *tx_len) {
    if (rx_len < 8) {
        *tx_len = 0;
        return;
    }

    /* MBAP Header: TransactionID(2) | ProtocolID(2) | Length(2) | UnitID(1) */
    uint16_t transaction_id = (rx_data[0] << 8) | rx_data[1];
    uint16_t protocol_id    = (rx_data[2] << 8) | rx_data[3];
    uint16_t length         = (rx_data[4] << 8) | rx_data[5];
    uint8_t  unit_id        = rx_data[6];
    uint8_t  fc             = rx_data[7];

    (void)protocol_id;

    if (unit_id != MODBUS_DEVICE_ID && unit_id != 0xFF) {
        *tx_len = 0;
        return;
    }

    /* Copy MBAP header to response */
    memcpy(tx_data, rx_data, 7);

    uint16_t addr     = (rx_data[8] << 8) | rx_data[9];
    uint16_t quantity = 0;
    if (rx_len >= 12) {
        quantity = (rx_data[10] << 8) | rx_data[11];
    }

    switch (fc) {
    case MODBUS_FC_READ_HOLDING: {
        if (addr + quantity > REG_COUNT) {
            tx_data[7] = fc | 0x80;
            tx_data[8] = MODBUS_EXCEPTION_ILLEGAL_ADDR;
            tx_data[4] = 0;
            tx_data[5] = 3;
            *tx_len = 9;
            return;
        }

        uint8_t byte_count = quantity * 2;
        tx_data[7] = fc;
        tx_data[8] = byte_count;

        for (uint16_t i = 0; i < quantity; i++) {
            uint16_t val = Modbus_ReadHoldingReg(addr + i);
            tx_data[9 + i * 2]       = (val >> 8) & 0xFF;
            tx_data[9 + i * 2 + 1]   = val & 0xFF;
        }

        uint16_t total_len = 3 + byte_count;
        tx_data[4] = (total_len >> 8) & 0xFF;
        tx_data[5] = total_len & 0xFF;
        *tx_len = 9 + quantity * 2;
        break;
    }

    case MODBUS_FC_WRITE_SINGLE: {
        if (addr >= REG_COUNT) {
            tx_data[7] = fc | 0x80;
            tx_data[8] = MODBUS_EXCEPTION_ILLEGAL_ADDR;
            tx_data[4] = 0;
            tx_data[5] = 3;
            *tx_len = 9;
            return;
        }

        uint16_t value = (rx_data[10] << 8) | rx_data[11];
        Modbus_WriteHoldingReg(addr, value);

        memcpy(tx_data + 7, rx_data + 7, 6);
        tx_data[4] = 0;
        tx_data[5] = 6;
        *tx_len = 13;
        break;
    }

    case MODBUS_FC_WRITE_MULTIPLE: {
        uint8_t byte_count = rx_data[12];
        uint16_t reg_count = byte_count / 2;

        if (addr + reg_count > REG_COUNT) {
            tx_data[7] = fc | 0x80;
            tx_data[8] = MODBUS_EXCEPTION_ILLEGAL_ADDR;
            tx_data[4] = 0;
            tx_data[5] = 3;
            *tx_len = 9;
            return;
        }

        for (uint16_t i = 0; i < reg_count; i++) {
            uint16_t value = (rx_data[13 + i * 2] << 8) | rx_data[14 + i * 2];
            Modbus_WriteHoldingReg(addr + i, value);
        }

        tx_data[7] = fc;
        tx_data[8] = rx_data[8];
        tx_data[9] = rx_data[9];
        tx_data[10] = rx_data[10];
        tx_data[11] = rx_data[11];
        tx_data[4] = 0;
        tx_data[5] = 6;
        *tx_len = 12;
        break;
    }

    default:
        tx_data[7] = fc | 0x80;
        tx_data[8] = MODBUS_EXCEPTION_ILLEGAL_FC;
        tx_data[4] = 0;
        tx_data[5] = 3;
        *tx_len = 9;
        break;
    }
}

/* ==================== Register Access ==================== */
uint16_t Modbus_ReadHoldingReg(uint16_t address) {
    if (address >= REG_COUNT) return 0;
    return g_sys.modbus_holding_regs[address];
}

void Modbus_WriteHoldingReg(uint16_t address, uint16_t value) {
    if (address >= REG_COUNT) return;
    g_sys.modbus_holding_regs[address] = value;

    /* Propagate write-through to system state based on register address */
    if (address >= REG_SP_BASE && address < REG_SP_BASE + NUM_PID_CHANNELS) {
        uint8_t ch = address - REG_SP_BASE;
        float sp = ModbusReg_To_Float(value, 10.0f);
        PID_SetSetpoint(ch, sp);
    }
    else if (address >= REG_KP_BASE && address < REG_KP_BASE + NUM_PID_CHANNELS) {
        uint8_t ch = address - REG_KP_BASE;
        float kp = ModbusReg_To_Float(value, 100.0f);
        g_sys.pid_channels[ch].kp = kp;
    }
    else if (address >= REG_KI_BASE && address < REG_KI_BASE + NUM_PID_CHANNELS) {
        uint8_t ch = address - REG_KI_BASE;
        g_sys.pid_channels[ch].ki = ModbusReg_To_Float(value, 100.0f);
    }
    else if (address >= REG_KD_BASE && address < REG_KD_BASE + NUM_PID_CHANNELS) {
        uint8_t ch = address - REG_KD_BASE;
        g_sys.pid_channels[ch].kd = ModbusReg_To_Float(value, 100.0f);
    }
    else if (address >= REG_PID_MODE_BASE && address < REG_PID_MODE_BASE + NUM_PID_CHANNELS) {
        uint8_t ch = address - REG_PID_MODE_BASE;
        PID_SetMode(ch, (value == 1) ? PID_MODE_AUTOMATIC : PID_MODE_MANUAL);
    }
    else if (address >= REG_ATUNE_CMD && address < REG_ATUNE_CMD + NUM_PID_CHANNELS) {
        if (value == 1) {
            uint8_t ch = address - REG_ATUNE_CMD;
            AutoTune_Start(ch, ATUNE_METHOD_ZN_CLOSED);
        }
    }

    g_sys.modbus_regs_dirty = true;
}

uint16_t Modbus_CRC16(uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void Modbus_BuildException(uint8_t function_code, uint8_t exception_code, uint8_t *tx_data, uint16_t *tx_len) {
    tx_data[0] = MODBUS_DEVICE_ID;
    tx_data[1] = function_code | 0x80;
    tx_data[2] = exception_code;
    *tx_len = 5;
}
