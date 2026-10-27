#include "stm32f10x.h"
#include "MyI2C.h"
#include "..\System\Delay.h"
#define AT24C02_ADDR_W	0xA0
#define AT24C02_ADDR_R	0xA1
void AT24C02_Init(void)
{
	MyI2C_Init();
}
uint8_t AT24C02_ReadByte(uint8_t WordAddress)
{
	uint8_t Data;
	MyI2C_Start();
	MyI2C_SendByte(AT24C02_ADDR_W);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(WordAddress);
	MyI2C_ReceiveAck();
	MyI2C_Start();
	MyI2C_SendByte(AT24C02_ADDR_R);
	MyI2C_ReceiveAck();
	Data = MyI2C_ReceiveByte();
	MyI2C_SendAck(1);
	MyI2C_Stop();
	return Data;
}
void AT24C02_WriteByte(uint8_t WordAddress, uint8_t Data)
{
	MyI2C_Start();
	MyI2C_SendByte(AT24C02_ADDR_W);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(WordAddress);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(Data);
	MyI2C_ReceiveAck();
	MyI2C_Stop();
	Delay_ms(5);
}
void AT24C02_ReadBytes(uint8_t WordAddress, uint8_t *Buffer, uint8_t Length)
{
	uint8_t i;
	MyI2C_Start();
	MyI2C_SendByte(AT24C02_ADDR_W);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(WordAddress);
	MyI2C_ReceiveAck();
	MyI2C_Start();
	MyI2C_SendByte(AT24C02_ADDR_R);
	MyI2C_ReceiveAck();
	for (i = 0; i < Length; i++)
	{
		Buffer[i] = MyI2C_ReceiveByte();
		if (i == Length - 1)
			MyI2C_SendAck(1);
		else
			MyI2C_SendAck(0);
	}
	MyI2C_Stop();
}
void AT24C02_WriteBytes(uint8_t WordAddress, uint8_t *Buffer, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)
	{
		AT24C02_WriteByte(WordAddress + i, Buffer[i]);
	}
}
