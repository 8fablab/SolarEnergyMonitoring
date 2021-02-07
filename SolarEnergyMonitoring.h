/*
 * SolarEnergyMonitoring.h
 *
 *  Created on: 4 févr. 2021
 *      Author: TooT
 */

#ifndef SOLARENERGYMONITORING_H_
#define SOLARENERGYMONITORING_H_

#include "Arduino.h"
#include "VE_MPPT.h"
#include "VEDirect.h"
#include <SD.h>
#include <Wire.h>
#include <RTClib.h>

void handleLog(uint32_t dt);
void handleLoad(uint64_t dt);
void handleCoulombCounter(uint64_t dt);
String formatDate();
void setDate(char* date);
void adjustDate();
void logEvent(uint8_t Code, String e);

void saveCapacity();
void readCapacity();

enum Event{
	Error = 1 << 0,
	Warn = 1 << 1,
	Info = 1 << 2,
	MinCap = 1 << 3,
	MaxTime = 1 << 4,
	MinVolt = 1 << 5,
	Charge = 1 << 6,
	Discharge = 1 << 7
};



#endif /* SOLARENERGYMONITORING_H_ */
