#ifndef __AT24C02_H
#define __AT24C02_H
void AT24C02_Init(void);
uint8_t AT24C02_ReadByte(uint8_t WordAddress);
void AT24C02_WriteByte(uint8_t WordAddress, uint8_t Data);
void AT24C02_ReadBytes(uint8_t WordAddress, uint8_t *Buffer, uint8_t Length);
void AT24C02_WriteBytes(uint8_t WordAddress, uint8_t *Buffer, uint8_t Length);
#endif
