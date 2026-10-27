#include "stm32f10x.h"
#include "..\System\Delay.h"
#include "OLED.h"
#include "DHT22.h"
#include "LED.h"
#include "Buzzer.h"
#include "Serial.h"
#include "AT24C02.h"
#include <string.h>
#include <stdlib.h>
#define ADDR_TEMP_LOW		0x00
#define ADDR_TEMP_HIGH		0x01
#define ADDR_HUMI_LOW		0x02
#define ADDR_HUMI_HIGH		0x03
#define ADDR_CHECK_FLAG		0x08
#define FLAG_INITIALIZED	0xAA
#define DEFAULT_TEMP_LOW	10
#define DEFAULT_TEMP_HIGH	26
#define DEFAULT_HUMI_LOW	20
#define DEFAULT_HUMI_HIGH	70
float Temperature, Humidity;
uint8_t DHT22_Error;
uint8_t HasValidData = 0;/*0=还没成功读到过数据，1=已经成功读到过*/
typedef enum {
	STATUS_NORMAL		= 0,
	STATUS_WARNING		= 1,
	STATUS_DANGER		= 2
} EnvStatus_t;
EnvStatus_t CurrentStatus = STATUS_NORMAL;
EnvStatus_t LastStatus = STATUS_NORMAL;
uint8_t TempLow, TempHigh, HumiLow, HumiHigh;
uint16_t SensorTimer = 0;
uint16_t BlinkTimer = 0;
uint16_t BuzzerTimer = 0;
uint8_t Threshold_Save(void)//把当前阈值写入AT24C02并校验，0=成功，1=失败
{
	AT24C02_WriteByte(ADDR_TEMP_LOW,  TempLow);
	AT24C02_WriteByte(ADDR_TEMP_HIGH, TempHigh);
	AT24C02_WriteByte(ADDR_HUMI_LOW,  HumiLow);
	AT24C02_WriteByte(ADDR_HUMI_HIGH, HumiHigh);
	AT24C02_WriteByte(ADDR_CHECK_FLAG, FLAG_INITIALIZED);
	if (AT24C02_ReadByte(ADDR_TEMP_LOW)   != TempLow)  return 1;
	if (AT24C02_ReadByte(ADDR_TEMP_HIGH)  != TempHigh) return 1;
	if (AT24C02_ReadByte(ADDR_HUMI_LOW)   != HumiLow)  return 1;
	if (AT24C02_ReadByte(ADDR_HUMI_HIGH)  != HumiHigh) return 1;
	if (AT24C02_ReadByte(ADDR_CHECK_FLAG) != FLAG_INITIALIZED) return 1;
	return 0;
}
void Threshold_Init(void)//从AT24C02加载阈值，异常时退回默认值
{
	if (AT24C02_ReadByte(ADDR_CHECK_FLAG) == FLAG_INITIALIZED)
	{
		/*AT24C02里已有保存过的阈值，直接读取*/
		TempLow  = AT24C02_ReadByte(ADDR_TEMP_LOW);
		TempHigh = AT24C02_ReadByte(ADDR_TEMP_HIGH);
		HumiLow  = AT24C02_ReadByte(ADDR_HUMI_LOW);
		HumiHigh = AT24C02_ReadByte(ADDR_HUMI_HIGH);
	}
	else
	{
		/*第一次上电（或AT24C02未接），先按默认值运行并尝试写入*/
		TempLow  = DEFAULT_TEMP_LOW;
		TempHigh = DEFAULT_TEMP_HIGH;
		HumiLow  = DEFAULT_HUMI_LOW;
		HumiHigh = DEFAULT_HUMI_HIGH;
		Threshold_Save();
	}
	/*读取到的数据不合法（0xFF/顺序颠倒/超范围）则退回默认值*/
	if (TempLow == 0xFF || TempHigh == 0xFF || HumiLow == 0xFF || HumiHigh == 0xFF ||
		TempLow >= TempHigh || HumiLow >= HumiHigh ||
		TempHigh > 100 || HumiHigh > 100)
	{
		TempLow  = DEFAULT_TEMP_LOW;
		TempHigh = DEFAULT_TEMP_HIGH;
		HumiLow  = DEFAULT_HUMI_LOW;
		HumiHigh = DEFAULT_HUMI_HIGH;
	}
}
EnvStatus_t EvaluateStatus(float temp, float humi)//判断当前状态
{
	EnvStatus_t tempLevel = STATUS_NORMAL;
	EnvStatus_t humiLevel = STATUS_NORMAL;
	/*温度判断*/
	if (temp < TempLow)
	{
		if (temp < TempLow - 8)		tempLevel = STATUS_DANGER;//低于下限8度则DANGER
		else						tempLevel = STATUS_WARNING;//否则则WARNING
	}
	else if (temp > TempHigh)
	{
		if (temp > TempHigh + 9)	tempLevel = STATUS_DANGER;//高于上限9度则DANGER
		else						tempLevel = STATUS_WARNING;//否则则WARNING
	}
	else
	{
		tempLevel = STATUS_NORMAL;//除此则正常
	}
	/*湿度判断同理*/
	if (humi < HumiLow)
	{
		if (humi < HumiLow - 10)	humiLevel = STATUS_DANGER;
		else						humiLevel = STATUS_WARNING;
	}
	else if (humi > HumiHigh)
	{
		if (humi > HumiHigh + 15)	humiLevel = STATUS_DANGER;
		else						humiLevel = STATUS_WARNING;
	}
	else
	{
		humiLevel = STATUS_NORMAL;
	}
	if (tempLevel == STATUS_DANGER || humiLevel == STATUS_DANGER)//温湿度有一个为DANGER则返回DANGER（先执行DANGER，确保DANGER的优先级更高）
		return STATUS_DANGER;
	if (tempLevel == STATUS_WARNING || humiLevel == STATUS_WARNING)//温湿度有一个为WARNING则返回WARNING
		return STATUS_WARNING;
	return STATUS_NORMAL;//除此则正常
}
void UpdateSensor(void)//错误状态检测
{
	float temp, humi;
	DHT22_Error = DHT22_ReadData(&temp, &humi);
	if (!DHT22_Error)//只有读取成功才更新保存值，失败时保留上一次的结果
	{
		Temperature = temp;
		Humidity = humi;
		HasValidData = 1;
	}
}
void UpdateOLED(void)
{
	int16_t tempInt, humiInt;
	uint8_t tempDec, humiDec;
	OLED_ShowString(1, 1, "T&H Monitor");
	if (!HasValidData)
	{
		/*开机后还没成功读到过数据，显示占位符*/
		OLED_ShowString(2, 1, "Temp: --.-C");
		OLED_ShowString(3, 1, "Humi: --.-%");
		OLED_ShowString(4, 1, "Waiting...      ");
		return;
	}
	/*正温度数据处理*/
	tempInt = (int16_t)Temperature;//取整数部分
	tempDec = (uint8_t)((Temperature - tempInt) * 10);//取小数点后第一位
	if (tempDec > 9) tempDec = 0;//防溢出保护
	OLED_ShowString(2, 1, "Temp: ");
	/*负温度数据处理*/
	if (tempInt < 0) { OLED_ShowChar(2, 7, '-'); tempInt = -tempInt; }//如果为负数则固定显示负号，并去绝对值
	else { OLED_ShowChar(2, 7, ' '); }//否则不显示负号
	OLED_ShowNum(2, 8, (uint16_t)tempInt, 2);
	OLED_ShowChar(2, 10, '.');
	OLED_ShowNum(2, 11, tempDec, 1);
	OLED_ShowChar(2, 12, 0xF8);
	OLED_ShowChar(2, 13, 'C');
	/*湿度数据处理，与正温度同理*/
	humiInt = (int16_t)Humidity;
	humiDec = (uint8_t)((Humidity - humiInt) * 10);
	if (humiDec > 9) humiDec = 0;
	OLED_ShowString(3, 1, "Humi: ");
	OLED_ShowNum(3, 7, (uint16_t)humiInt, 2);
	OLED_ShowChar(3, 9, '.');
	OLED_ShowNum(3, 10, humiDec, 1);
	OLED_ShowChar(3, 11, '%');
	/*状态提示显示*/
	switch (CurrentStatus)
	{
		case STATUS_NORMAL:
			OLED_ShowString(4, 1, "Comfortable  ");
			break;
		case STATUS_WARNING:
			OLED_ShowString(4, 1, "Uncomfortable");
			break;
		case STATUS_DANGER:
			OLED_ShowString(4, 1, "!DANGER! Exit");
			break;
	}
}
void UpdateLED_Buzzer(void)//LED和蜂鸣器机制
{
	switch (CurrentStatus)
	{
		case STATUS_NORMAL:
			LED1_ON();
			LED2_OFF();
			LED3_OFF();
			Buzzer_OFF();
			break;
		case STATUS_WARNING:
			LED1_OFF();
			if (BlinkTimer >= 500)
			{
				BlinkTimer = 0;
				LED2_Turn();
			}
			LED3_OFF();
			Buzzer_OFF();
			break;
		case STATUS_DANGER:
			LED1_OFF();
			LED2_OFF();
			if (BlinkTimer >= 200)
			{
				BlinkTimer = 0;
				LED3_Turn();
			}
			if (BuzzerTimer >= 250)
			{
				BuzzerTimer = 0;
				Buzzer_Turn();
			}
			break;
	}
}
void ProcessSerialCommand(void)
{
	uint8_t *cmd = Serial_GetRxLine();
	if (!Serial_GetRxLineFlag()) return;
	Serial_SendString("-> ");
	Serial_SendString((char *)cmd);
	Serial_NewLine();
	if (strcmp((char *)cmd, "READ") == 0)//读取当前阈值
	{
		Serial_Printf("Thresholds (AT24C02):\r\n");
		Serial_Printf("  Temp: %d ~ %d C\r\n", TempLow, TempHigh);
		Serial_Printf("  Humi: %d ~ %d %%\r\n", HumiLow, HumiHigh);
	}
	else if (strcmp((char *)cmd, "DEFAULT") == 0)//恢复默认值
	{
		TempLow = DEFAULT_TEMP_LOW;
		TempHigh = DEFAULT_TEMP_HIGH;
		HumiLow = DEFAULT_HUMI_LOW;
		HumiHigh = DEFAULT_HUMI_HIGH;
		if (Threshold_Save() == 0)
			Serial_SendString("Default thresholds saved to AT24C02.\r\n");
		else
			Serial_SendString("Default restored, but AT24C02 save failed!\r\n");
	}
	else if (strncmp((char *)cmd, "SET_TEMP_LOW ", 13) == 0)//设置温度下限
	{
		int val = atoi((char *)cmd + 13);
		if (val > 0 && val < TempHigh) {
			TempLow = (uint8_t)val;
			if (Threshold_Save() == 0)
				Serial_Printf("Temp LOW set to %d C (saved)\r\n", TempLow);
			else
				Serial_Printf("Temp LOW set to %d C (save failed!)\r\n", TempLow);
		} else { Serial_SendString("Invalid value!\r\n"); }
	}
	else if (strncmp((char *)cmd, "SET_TEMP_HIGH ", 14) == 0)//设置温度上限
	{
		int val = atoi((char *)cmd + 14);
		if (val > TempLow && val < 100) {
			TempHigh = (uint8_t)val;
			if (Threshold_Save() == 0)
				Serial_Printf("Temp HIGH set to %d C (saved)\r\n", TempHigh);
			else
				Serial_Printf("Temp HIGH set to %d C (save failed!)\r\n", TempHigh);
		} else { Serial_SendString("Invalid value!\r\n"); }
	}
	else if (strncmp((char *)cmd, "SET_HUMI_LOW ", 13) == 0)//设置湿度下限
	{
		int val = atoi((char *)cmd + 13);
		if (val >= 0 && val < HumiHigh) {
			HumiLow = (uint8_t)val;
			if (Threshold_Save() == 0)
				Serial_Printf("Humi LOW set to %d %% (saved)\r\n", HumiLow);
			else
				Serial_Printf("Humi LOW set to %d %% (save failed!)\r\n", HumiLow);
		} else { Serial_SendString("Invalid value!\r\n"); }
	}
	else if (strncmp((char *)cmd, "SET_HUMI_HIGH ", 14) == 0)//设置湿度上限
	{
		int val = atoi((char *)cmd + 14);
		if (val > HumiLow && val <= 100) {
			HumiHigh = (uint8_t)val;
			if (Threshold_Save() == 0)
				Serial_Printf("Humi HIGH set to %d %% (saved)\r\n", HumiHigh);
			else
				Serial_Printf("Humi HIGH set to %d %% (save failed!)\r\n", HumiHigh);
		} else { Serial_SendString("Invalid value!\r\n"); }
	}
	else
	{
		Serial_SendString("Unknown. Cmds: READ DEFAULT SET_TEMP_LOW/HIGH SET_HUMI_LOW/HIGH\r\n");//读取到未知命令，显示可用命令
	}
}
int main(void)
{
	OLED_Init();
	AT24C02_Init();
	Threshold_Init();
	DHT22_Init();
	LED_Init();
	Buzzer_Init();
	Serial_Init();
	OLED_ShowString(1, 1, " T&H Monitor  ");
	OLED_ShowString(2, 1, "Initializing...");
	Serial_SendString("T&H Monitor System Ready\r\n");
	Delay_ms(1500);
	OLED_Clear();
	UpdateSensor();
	while (1)
	{
		Delay_ms(10);
		SensorTimer += 10;
		if (SensorTimer >= 2000)
		{
			SensorTimer = 0;
			UpdateSensor();
			if (!DHT22_Error)
			{
				LastStatus = CurrentStatus;
				CurrentStatus = EvaluateStatus(Temperature, Humidity);
			}
			UpdateOLED();
		}
		BlinkTimer += 10;
		BuzzerTimer += 10;
		UpdateLED_Buzzer();
		ProcessSerialCommand();
	}
}
