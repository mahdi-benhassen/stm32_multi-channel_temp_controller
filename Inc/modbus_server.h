#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include "data_structures.h"
#include <stdint.h>

/* ==================== Modbus Frame Definitions ==================== */
#define MODBUS_RTU_FRAME_MAX        256
#define MODBUS_TCP_FRAME_MAX        260  /* 256 + MBAP header (7) - 3 */

#define MODBUS_FC_READ_HOLDING      0x03
#define MODBUS_FC_WRITE_SINGLE      0x06
#define MODBUS_FC_WRITE_MULTIPLE    0x10
#define MODBUS_FC_READ_INPUT        0x04

#define MODBUS_EXCEPTION_ILLEGAL_FC   1
#define MODBUS_EXCEPTION_ILLEGAL_ADDR 2
#define MODBUS_EXCEPTION_ILLEGAL_VAL  3
#define MODBUS_EXCEPTION_SLAVE_FAIL   4

typedef enum {
    MODBUS_OK        = 0,
    MODBUS_ERR_CRC   = 1,
    MODBUS_ERR_FRAME = 2,
    MODBUS_ERR_ADDR  = 3
} ModbusStatus_t;

/* ==================== Functions ==================== */
void            Modbus_Init(void);
void            Modbus_RTU_Process(void);
void            Modbus_TCP_Process(uint8_t *rx_data, uint16_t rx_len, uint8_t *tx_data, uint16_t *tx_len);

ModbusStatus_t  Modbus_Parse_RTU_Frame(uint8_t *frame, uint16_t len);
ModbusStatus_t  Modbus_Parse_TCP_Frame(uint8_t *frame, uint16_t len);

uint16_t        Modbus_ReadHoldingReg(uint16_t address);
void            Modbus_WriteHoldingReg(uint16_t address, uint16_t value);
uint16_t        Modbus_CRC16(uint8_t *data, uint16_t len);
void            Modbus_BuildException(uint8_t function_code, uint8_t exception_code, uint8_t *tx_data, uint16_t *tx_len);

#endif /* MODBUS_SERVER_H */
