# SolarEnergyMonitoring

This project aims at monitoring a solar panel battery charge using a MPPT VE 75I10 charger and an Arduino MEga 2560 and some peripherals as a SD Card logger and an RTC.

It log the data read from the MPPT every 5mns and maintains a state of charge indicator through load current integration.
It also as the ability to discharge the battery through a resistor network until a certain SOC is reached.

It makes use of the [VictronVEDirectArduino](https://github.com/T88T/VictronVEDirectArduino) and the [VEDirectMPPTArduino](https://github.com/T88T/VEDirectMPPTArduino) libraries.

In the hope it could help someone !
