#include "SolarEnergyMonitoring.h"

using namespace VE_Manager;

#define LOAD_SET 43
#define LOAD_RESET 42

#define LOG_TIME 1000.f*60.f*5.f  //5mn
#define MIN_VOLTAGE 10.2 // 10.2V Minimum
#define DEPTH_OF_DISCHARGE 0.4f // 40% DOD
#define MAX_DISCHARGE_TIME uint32_t(1000)*uint32_t(60)*uint32_t(326) // 326mn --> Max discharge time to burn 60% of capacity @ 4.2A
#define COULOMB_TIME float(1000*10) // 10s
#define MAX_CAPACITY 38000.f // mAh
#define COULOMB_COEFF float(COULOMB_TIME/(3600.f*1000.f))
#define NIGHT_DELAY uint32_t(1000)*uint32_t(60)*uint32_t(60) // 60mn before switching to night mode

float Capacity = MAX_CAPACITY/2;

VE_MPPT *MPPT; // added a fixed leak current of 0.1A
RTC_DS3231 RTC;
DateTime Date;

File EventFile;
File MPPTFile;
File SOCFile;
File CapacityFile;

uint64_t previousTime = 0;
uint64_t logTimer = 0;
uint64_t coulombTimer = 0;
uint64_t verboseTimer = 0;
uint64_t nightTimer = 0;
uint64_t dischargeTimer = 0;

enum Load{Off, StartDischarging, Discharging, StopDischarging, Discharged};
bool isNight = false;
uint8_t LoadState = Load::Off;

bool Verbose = false;

enum Reset{None = 0,
	SD_Insertion = 2,
	MPPT_Connection = 8
};

int boot = 0;

void setup()
{
	Serial.begin(19200);

	pinMode(LOAD_SET, OUTPUT);
	pinMode(LOAD_RESET, OUTPUT);

	Serial.println("Switching off load");
	digitalWrite(LOAD_RESET, HIGH);
	delay(300);

	digitalWrite(LOAD_RESET, LOW);
	digitalWrite(LOAD_SET, LOW);

	// Connection to MPPT device
	MPPT = new VE_MPPT(Serial1);
	Serial.print("MPPT connection... ");
	if(!MPPT->isConnected())
	{
		Serial.println("MPPT connection failed... ");
		delay(5000);
		boot |= Reset::MPPT_Connection;
		setup();
	}
	else
		Serial.println("OK");

	//Checking for SD card available
	Serial.print("SD connection... ");
	if(!SD.begin(49))
	{
		Serial.println("Insert SD card");

		while(!SD.begin(49))
		{
			delay(2000);
		}
	}
	else
		Serial.println("OK");

	if(SD.open("Events.log", FILE_WRITE))
	{
		Serial.println("\tEvents.log available for write");
	}
	else
	{
		Serial.println("Events.log not available for write... Is SD card locked ?\n\rResetting... ");
		delay(5000);
		boot |= Reset::SD_Insertion;
		setup();
	}

	if(SD.open("SOC.log", FILE_WRITE))
	{
		Serial.println("\tSOC.log available for write");
	}
	else
	{
		Serial.println("SOC.log not available for write... Is SD card locked ?\n\rResetting... ");
		delay(5000);
		boot |= Reset::SD_Insertion;
		setup();
	}

	if(SD.open("MPPT.log", FILE_WRITE))
		Serial.println("\tMPPT.log available for write");
	else
	{
		Serial.println("MPPT.log not available for write... Is SD card locked ?\n\rResetting... ");
		delay(5000);
		boot |= Reset::SD_Insertion;
		setup();
	}

	if(SD.open("Capacity.log", FILE_WRITE))
		Serial.println("\tMPPT.log available for write");
	else
	{
		Serial.println("MPPT.log not available for write... Is SD card locked ?\n\rResetting... ");
		delay(5000);
		boot |= Reset::SD_Insertion;
		setup();
	}

	Serial.println("Loading saved capacity : ");
	readCapacity();

	//Initializing RTC with time correction possibility
	Wire.begin();

	adjustDate();
	Serial.println(String("Init finished starting at ") + formatDate());

	if(!boot)
		logEvent(Event::Info, "Boot ok");
	else
	{
		if(boot & Reset::MPPT_Connection)
			logEvent(Event::Error, "Reset : Unable to connect to MPPT");
		if(boot & Reset::SD_Insertion)
			logEvent(Event::Error, "Reset : SD_Insertion");

		boot = 0;
	}

}


void loop()
{
	MPPT->update();

	uint64_t currentTime = millis();
	uint64_t dt = currentTime - previousTime;
	previousTime = currentTime;
	verboseTimer += dt;

	if(Serial.read() == 'v')
	{
		Verbose = !Verbose;
		Verbose ? Serial.println("Verbose mode : ON") : Serial.println("Verbose Mode : OFF");
	}

	if(verboseTimer > 5000)
	{
		verboseTimer = 0;
		if(Verbose)
		{
			Serial.println("#### Verbose dump : " + formatDate() + " ####");
			Serial.print("MainBatteryVoltage : "); Serial.print(MPPT->MainBatteryVoltage, 2); Serial.println(" V");
			Serial.print("PanelVoltage : "); Serial.print(MPPT->PanelVoltage, 2); Serial.println(" V");
			Serial.print("PanelPower : "); Serial.print(MPPT->PanelPower, 2); Serial.println(" W");
			Serial.print("BatteryCurrent : "); Serial.print(MPPT->BatteryCurrent, 2); Serial.println(" A");
			Serial.print("LoadCurrent : "); Serial.print(MPPT->LoadCurrent, 2); Serial.println(" A");
			Serial.println();
			Serial.print("Capacity : "); Serial.print(Capacity, 2); Serial.println(" mAh");
			Serial.print("Load state : "); Serial.println(LoadState);
			Serial.println("####"); Serial.println();
		}
	}

	handleLog(dt);
	handleCoulombCounter(dt);
	handleLoad(dt);

	delay(10);
}


void handleLog(uint32_t dt)
{
	logTimer += dt;

	if(logTimer > LOG_TIME)
	{
		logTimer = 0;

		if(!MPPTFile)
			MPPTFile.close();

		MPPTFile = SD.open("MPPT.log", FILE_WRITE);

		if(MPPTFile)
		{
			MPPTFile.seek(EOF);
			MPPTFile.println(formatDate() + "," + MPPT->getData());
			MPPTFile.close();
		}

		else
		{
			boot |= Reset::SD_Insertion;
			setup();
		}

		if(!SOCFile)
			SOCFile.close();

		SOCFile = SD.open("SOC.log", FILE_WRITE);

		if(SOCFile)
		{
			SOCFile.seek(EOF);
			SOCFile.println(formatDate() + "," + String(Capacity,2));
			SOCFile.close();
		}

		else
		{
			boot |= Reset::SD_Insertion;
			setup();
		}
	}
}

void handleLoad(uint64_t dt)
{
	if(MPPT->PanelPower > 0.f)
	{
		nightTimer = 0;

		if(isNight)
			logEvent(Event::Info, "Rise and shine !");

		isNight = false;

		if(LoadState == Load::Discharging || LoadState == Load::StartDischarging)
			LoadState = Load::StopDischarging;
	}

	nightTimer += dt;

	if(nightTimer > NIGHT_DELAY)
	{
		if(isNight == false)
		{
			logEvent(Event::Info, "The night as come");
			LoadState = Load::StartDischarging;
		}

		isNight = true;
	}

	if(isNight)
		nightTimer = 0;


	switch(LoadState)
	{
	case Load::Off:
		digitalWrite(LOAD_RESET, HIGH);
		delay(10);
		digitalWrite(LOAD_RESET, LOW);
		break;

	case Load::StartDischarging:
		digitalWrite(LOAD_SET, HIGH);
		delay(10);
		digitalWrite(LOAD_SET, LOW);
		dischargeTimer = 0;
		logEvent(Event::Discharge, "Start forced discharge");
		LoadState = Load::Discharging;
		break;

	case Load::Discharging:
		if(Capacity < (MAX_CAPACITY*DEPTH_OF_DISCHARGE))
		{
			LoadState = Load::StopDischarging;
			logEvent(Event::MinCap, "Stop forced discharged : Min capacity reached");
		}

		dischargeTimer += dt;

		if(dischargeTimer > MAX_DISCHARGE_TIME)
		{
			LoadState = Load::StopDischarging;
			logEvent(Event::MaxTime, "Stop forced discharged : Max time reached");
		}

		if(MPPT->MainBatteryVoltage < MIN_VOLTAGE)
		{
			LoadState = Load::StopDischarging;
			logEvent(Event::MinVolt, "Stop forced discharged : Min voltage reached");
		}

		break;

	case Load::StopDischarging:
		digitalWrite(LOAD_RESET, HIGH);
		delay(10);
		digitalWrite(LOAD_RESET, LOW);

		logEvent(Event::Charge, "Ending forced discharge");
		LoadState = Load::Discharged;
		break;

	case Load::Discharged:
		break;
	}

}


void handleCoulombCounter(uint64_t dt)
{
	coulombTimer += dt;

	if(coulombTimer > COULOMB_TIME)
	{
		coulombTimer = 0;
		float accumulated = MPPT->BatteryCurrent*1000.f*COULOMB_COEFF;

		accumulated = (Capacity+accumulated > 0.f) ? accumulated : 0.f;
		Capacity += accumulated;

		if(Capacity > MAX_CAPACITY)
			Capacity = MAX_CAPACITY;

		saveCapacity();
	}
}


