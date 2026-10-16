#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>
uint8_t Serial_RxData;
uint8_t Serial_RxFlag;
#define SERIAL_RX_BUF_SIZE	64
uint8_t Serial_RxBuf[SERIAL_RX_BUF_SIZE];
uint8_t Serial_RxBufIndex = 0;
uint8_t Serial_RxLineFlag = 0;
void Serial_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;//波特率
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//不使用硬件流控功能
	USART_InitStructure.USART_Mode = USART_Mode_Tx|USART_Mode_Rx;//输出和输入
	USART_InitStructure.USART_Parity = USART_Parity_No;//无校验
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//1位停止位
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//8位字长
	USART_Init(USART1, &USART_InitStructure);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);
	USART_Cmd(USART1, ENABLE);
}
void Serial_SendByte(uint8_t Byte)//发送字符类型数据
{
	USART_SendData(USART1, Byte);//发送一字节数据
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);//发送完成检测TXE标志位
}
void Serial_SendArray(uint8_t *Array, uint16_t Length)//发送数组类型数据
{
	uint16_t i;
	for (i = 0; i < Length; i++)//遍历数组
	{
		Serial_SendByte(Array[i]);
	}
}
void Serial_SendString(char *String)//发送字符串类型的数据
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		Serial_SendByte(String[i]);
	}
}
uint32_t Serial_Pow(uint32_t x, uint32_t y)//把参数以x^y的形式返回
{
	uint32_t result = 1;
	while (y--) result *= x;
	return result;
}
void Serial_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)
	{
		Serial_SendByte(Number / Serial_Pow(10, Length - i - 1) % 10 + '0');//把整数的每一位取出来，并加上'0'使其以字符的形式显示
	}
}
void Serial_NewLine(void)//换行函数
{
	Serial_SendString("\r\n");
}
int fputc(int ch, FILE *f)
{
	Serial_SendByte(ch);
	return ch;
}
void Serial_Printf(char *format, ...)
{
	char String[100];
	va_list arg;
	va_start(arg, format);
	vsprintf(String, format, arg);
	va_end(arg);
	Serial_SendString(String);
}
uint8_t Serial_GetRxFlag(void)//接收到数据，标志位自动清零
{
	if (Serial_RxFlag == 1)
	{
		Serial_RxFlag = 0;
		return 1;
	}
	return 0;
}
uint8_t Serial_GetRxData(void)//返回接收到的单字节数据
{
	return Serial_RxData;
}
uint8_t Serial_GetRxLineFlag(void)//接收到数据，标志位自动清除
{
	if (Serial_RxLineFlag == 1)
	{
		Serial_RxLineFlag = 0;
		return 1;
	}
	return 0;
}
uint8_t *Serial_GetRxLine(void)//返回整行数据
{
	return Serial_RxBuf;
}
void USART1_IRQHandler(void)
{
	uint8_t ch;
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)//RXNE置1，表示接收到数据
	{
		ch = USART_ReceiveData(USART1);
		Serial_RxData = ch;//把读取的数据赋给Serial_RxData
		Serial_RxFlag = 1;
		if (ch == '\r' || ch == '\n')//判断是否为结束字符
		{
			if (Serial_RxBufIndex > 0)//判断缓冲区是否有内容
			{
				Serial_RxBuf[Serial_RxBufIndex] = '\0';//补上结束符
				Serial_RxBufIndex = 0;//索引归零，准备收下一行
				Serial_RxLineFlag = 1;//置位“整行到达”标志
			}
		}
		else//如果不是则拼接数组
		{
			if (Serial_RxBufIndex < SERIAL_RX_BUF_SIZE - 1)//防溢出
			{
				Serial_RxBuf[Serial_RxBufIndex++] = ch;//将读取的数据存入数组，索引后移
			}
		}
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}
/*
1.USART每次执行中断函数都只能读取一个字节数据，然后将读取到的数据组合
2.在发送整数时，需要把整数的每一位取出来，具体操作为：把数据除以10^ (Length - i - 1)然后再对结果取余
*/
