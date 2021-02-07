#include "Arduino.h"
#include "RTClib.h"
#include "SD.h"

extern RTC_DS1307 RTC;
extern DateTime Date;
extern File EventFile;
extern File CapacityFile;

extern float Capacity;

extern int boot;
extern bool Verbose;

enum Event{
	Error = 1 << 0,
	Warn = 1 << 1,
	Info = 1 << 2,
	MinCap = 1 << 3,
	MaxTime = 1 << 4,
	MinVolt = 1 << 5,
	Chargring = 1 << 6,
	Discharging = 1 << 7
};

enum Reset{None = 0,
	SD_Insertion = 2,
	MPPT_Connection = 8
};

String formatDate()
{
	Date = RTC.now();
	return String(String(Date.day()) + String("/") + String(Date.month()) + String("/") + String(Date.year()) + " " + Date.hour() + String(":") + String(Date.minute()));
}

void setDate(char* date)
{

	byte Temp1, Temp2;
	int Hour, Minute, Day, Month, Year;

	// Read Year first
	Temp1 = (int)date[0] - 48;
	Temp2 = (int)date[1] - 48;
	Hour = Temp1 * 10 + Temp2;

	// now month
	Temp1 = (int)date[3] - 48;
	Temp2 = (int)date[4] - 48;
	Minute = Temp1 * 10 + Temp2;

	// now date
	Temp1 = (int)date[6] - 48;
	Temp2 = (int)date[7] - 48;
	Day = Temp1 * 10 + Temp2;

	// now Hour
	Temp1 = (int)date[9] - 48;
	Temp2 = (int)date[10] - 48;
	Month = Temp1 * 10 + Temp2;

	// now Minute
	Temp1 = (int)date[12] - 48;
	Temp2 = (int)date[13] - 48;
	Year = Temp1 * 10 + Temp2;

	Date = DateTime(Year, Month, Day, Hour, Minute);
	RTC.adjust(Date);

	Serial.print("\tNew date is : ");
	Serial.println(formatDate());
}

void adjustDate()
{
	int previousTime = millis();
	bool RTC_Correction = false;
	char RX_Buffer[16] = {0};
	uint8_t RX_Pointer = 0;

	//Scanning for new date correction ('x' to abort)
	Serial.print("Current Date is ");
	Date = RTC.now();
	Serial.println(formatDate());
	Serial.println("Starting RTC adjust... (x to skip)");
	Serial.println("\tRTC correction format : 'hh:mm DD/MM/YY'");
	while((millis() - previousTime) < 10000)
	{
		if(Serial.available())
		{
			RX_Buffer[RX_Pointer] = Serial.read();

			if(RX_Buffer[RX_Pointer] == 'x' || RX_Buffer[RX_Pointer] == 'X')
			{
				RTC_Correction = false;
				Serial.println("\tSkip RTC adjust");
				return;
			}

			RX_Pointer++;

			if(RX_Pointer > 13)
			{
				RTC_Correction = true;
				break;
			}

		}
	}

	if(RTC_Correction)
		setDate(RX_Buffer);
	else
		Serial.println("\tSkip RTC adjust");

}


void logEvent(uint8_t Code, String e)
{
	if(Verbose)
		Serial.println(String(Code) + ", " + formatDate() + " : " + e);

	if(!EventFile)
		EventFile.close();

	EventFile = SD.open("Events.log", FILE_WRITE);

	if(EventFile)
	{
		EventFile.seek(EOF);
		EventFile.println(String(Code) + ", " + formatDate() + " , " + e);
		EventFile.close();
	}

	else
	{
		boot |= Reset::SD_Insertion;
		setup();
	}
}

void readCapacity()
{
	CapacityFile = SD.open("Capacity.log", FILE_READ);

	if(CapacityFile)
	{
		String buffer = CapacityFile.readStringUntil(';');
		Serial.print("Read capacity : "); Serial.print(buffer); Serial.println(" mAh"); //Printing for debugging purpose
		Capacity = buffer.toFloat();

		CapacityFile.close();
	}

	else
	{
		boot |= Reset::SD_Insertion;
		setup();
	}
}

void saveCapacity()
{
	SD.remove("Capacity.log");
	CapacityFile = SD.open("Capacity.log", FILE_WRITE);

	if(CapacityFile)
	{
		CapacityFile.print(Capacity);
		CapacityFile.print(';');
		CapacityFile.close();
	}

	else
	{
		boot |= Reset::SD_Insertion;
		setup();
	}
}
