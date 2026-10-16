#include "stm32f10x.h"
#include "..\System\Delay.h"
#define DHT22_GPIO_PORT	GPIOA
#define DHT22_GPIO_PIN	GPIO_Pin_0
void DHT22_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = DHT22_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT22_GPIO_PORT, &GPIO_InitStructure);
	GPIO_SetBits(DHT22_GPIO_PORT, DHT22_GPIO_PIN);
}
/*设置DHT22为输出模式*/
static void DHT22_SetOutput(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;//设置为推挽输出，STM32主动拉高/拉低，向 DHT22 发指令
	GPIO_InitStructure.GPIO_Pin = DHT22_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT22_GPIO_PORT, &GPIO_InitStructure);
}
/*设置DHT22为输入模式*/
static void DHT22_SetInput(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;//设置为上拉输入，DHT22主动拉高/拉低，STM32 被动读取
	GPIO_InitStructure.GPIO_Pin = DHT22_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DHT22_GPIO_PORT, &GPIO_InitStructure);
}
/*检测DHT22是否达到想要的电平*/
static uint8_t DHT22_WaitPinState(uint8_t State, uint32_t TimeoutUs)
{
	while (GPIO_ReadInputDataBit(DHT22_GPIO_PORT, DHT22_GPIO_PIN) != State)//等待引脚成为State电平
	{
		if (TimeoutUs == 0) return 1;//如果超时则直接返回1，表示失败
		Delay_us(1);
		TimeoutUs--;
	}
	return 0;//返回0，表示成功
}
/*读取数据函数*/
uint8_t DHT22_ReadData(float *Temperature, float *Humidity)
{
	uint8_t retry;
	for (retry = 0; retry < 3; retry++)//重试三次读取数据
	{
		uint8_t i;
		uint8_t data[5] = {0,0,0,0,0};
		/*时序开始信号*/
		__disable_irq();// 关闭全局中断
		DHT22_SetOutput();
		GPIO_ResetBits(DHT22_GPIO_PORT, DHT22_GPIO_PIN);
		Delay_ms(18);
		GPIO_SetBits(DHT22_GPIO_PORT, DHT22_GPIO_PIN);
		Delay_us(5);
		DHT22_SetInput();
		__enable_irq();// 开启全局中断
		/*DHT22应答，若应答失败，则跳出本次循环*/
		if (DHT22_WaitPinState(0, 300)) continue;
		if (DHT22_WaitPinState(1, 300)) continue;
		/*开始读取数据：湿度高字节、湿度低字节、温度高字节、温度低字节、校验和*/
		for (i = 0; i < 40; i++)
		{
			if (DHT22_WaitPinState(0, 100)) { i = 40; break; }// 等待传感器把总线拉低（表示数据位开始）
			if (DHT22_WaitPinState(1, 100)) { i = 40; break; }// 等待传感器把总线拉高（开始输出数据电平）
			Delay_us(40);
			if (GPIO_ReadInputDataBit(DHT22_GPIO_PORT, DHT22_GPIO_PIN) == 1)
			{
				data[i / 8] |= (0x80 >> (i % 8));//若读取电平为1，则把第i / 8个元素的第i % 8位置1
			}
		}
		if (data[4] != (data[0] + data[1] + data[2] + data[3])) continue;//前四个字节相加，取低八位，与第五个校验位比较
		*Humidity = (float)(((uint16_t)data[0] << 8) | data[1]) / 10.0f;//将data[0，1]整合除以10传给湿度指针
		if (data[2] & 0x80)//判断温度正负，再进行整合，传给温度指针
			*Temperature = -(float)(((uint16_t)(data[2] & 0x7F) << 8) | data[3]) / 10.0f;
		else
			*Temperature = (float)(((uint16_t)data[2] << 8) | data[3]) / 10.0f;
		return 0;
	}
	return 1;
}
/*
1.DHT22时序：a.主机发起信号（主机将引脚设为输出，拉低电平 18ms）
			 b.主机释放总线（主机拉高总线，延时极短（20~40μs），然后立刻把引脚切换为输入模式，只延迟5μs捕获传感器接下来拉低总线的那个“下降沿瞬间”，避免漏掉传感器的开始信号）
			 c.传感器回应（当主机释放总线后，DHT22 会主动拉低总线80μs告诉主机“我收到了！”传感器拉高总线 80μs告诉主机“我要开始发数据了”）
			 d.数据传输（DHT22 的协议中，传输每一位数据，传感器都会重新走一遍“拉低总线 → 拉高总线”的流程，数据传输时数据“0”用50μs低电平，
			   26~28μs高电平来表示，“1”用50μs低电平，70μs高电平来表示）
2.为什么要关闭全局中断：是为了保证 DHT22 的握手时序不被中断干扰，保证时序的纯洁
3.for循环里的Delay_us(40)：因为高电平26~28μs表示“0”，高电平70μs表示“1”，延时40μs再读取电平即可辨别数据
*/
