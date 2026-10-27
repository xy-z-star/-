#ifndef __DHT22_H
#define __DHT22_H
#include <stdint.h>
void DHT22_Init(void);
uint8_t DHT22_ReadData(float *Temperature, float *Humidity);
#endif
