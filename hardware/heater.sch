EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "Heater controller"
Date "2020-04-24"
Rev "2"
Comp "François  Berder"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Diode:1N4007 D4
U 1 1 5DA8C3CF
P 9750 4400
F 0 "D4" H 9750 4184 50  0000 C CNN
F 1 "1N4007" H 9750 4275 50  0000 C CNN
F 2 "Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal" H 9750 4225 50  0001 C CNN
F 3 "http://www.vishay.com/docs/88503/1n4001.pdf" H 9750 4400 50  0001 C CNN
	1    9750 4400
	-1   0    0    1   
$EndComp
$Comp
L Diode:1N4007 D3
U 1 1 5DA8C781
P 6250 4400
F 0 "D3" H 6300 4650 50  0000 R CNN
F 1 "1N4007" H 6400 4550 50  0000 R CNN
F 2 "Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal" H 6250 4225 50  0001 C CNN
F 3 "http://www.vishay.com/docs/88503/1n4001.pdf" H 6250 4400 50  0001 C CNN
	1    6250 4400
	1    0    0    -1  
$EndComp
$Comp
L Connector:Screw_Terminal_01x03 J2
U 1 1 5DA8CD9C
P 2150 4600
F 0 "J2" H 2230 4642 50  0000 L CNN
F 1 "Screw_Terminal_01x03" H 2230 4551 50  0000 L CNN
F 2 "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-3-3-5.08_1x03_P5.08mm_Horizontal" H 2150 4600 50  0001 C CNN
F 3 "~" H 2150 4600 50  0001 C CNN
	1    2150 4600
	1    0    0    1   
$EndComp
$Comp
L IXYS:CPC1972 U2
U 1 1 5DA91CA2
P 5450 5050
F 0 "U2" H 5475 5965 50  0000 C CNN
F 1 "CPC1972" H 5475 5874 50  0000 C CNN
F 2 "Package_DIP:SMDIP-6_W7.62mm" H 5450 5050 50  0001 C CNN
F 3 "" H 5450 5050 50  0001 C CNN
	1    5450 5050
	1    0    0    -1  
$EndComp
$Comp
L IXYS:CPC1972 U3
U 1 1 5DA920A8
P 8950 5050
F 0 "U3" H 8975 5965 50  0000 C CNN
F 1 "CPC1972" H 8975 5874 50  0000 C CNN
F 2 "Package_DIP:SMDIP-6_W7.62mm" H 8950 5050 50  0001 C CNN
F 3 "" H 8950 5050 50  0001 C CNN
	1    8950 5050
	1    0    0    -1  
$EndComp
Wire Wire Line
	1950 4100 1900 4100
Text Label 1900 4100 2    50   ~ 0
LIVE
Text Label 1800 4200 2    50   ~ 0
NEUTRAL
Text Label 6550 4400 0    50   ~ 0
LIVE
Wire Wire Line
	5850 4400 6100 4400
Wire Wire Line
	6400 4400 6550 4400
Wire Wire Line
	9350 4400 9600 4400
Text Label 10050 4400 0    50   ~ 0
LIVE
Wire Wire Line
	10050 4400 9900 4400
Text Label 5850 4700 0    50   ~ 0
POSITIVE_OUTPUT
Text Label 9350 4700 0    50   ~ 0
NEGATIVE_OUTPUT
NoConn ~ 9350 4550
NoConn ~ 5850 4550
NoConn ~ 5100 4700
NoConn ~ 8600 4700
$Comp
L Device:R R4
U 1 1 5DAB56C1
P 4200 4400
F 0 "R4" V 3993 4400 50  0000 C CNN
F 1 "1k" V 4084 4400 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 4130 4400 50  0001 C CNN
F 3 "~" H 4200 4400 50  0001 C CNN
	1    4200 4400
	0    1    1    0   
$EndComp
$Comp
L power:GND #PWR016
U 1 1 5DAB84A9
P 8350 5000
F 0 "#PWR016" H 8350 4750 50  0001 C CNN
F 1 "GND" H 8355 4827 50  0000 C CNN
F 2 "" H 8350 5000 50  0001 C CNN
F 3 "" H 8350 5000 50  0001 C CNN
	1    8350 5000
	1    0    0    -1  
$EndComp
Wire Wire Line
	8600 4550 8350 4550
Wire Wire Line
	8350 4550 8350 4900
$Comp
L power:GND #PWR012
U 1 1 5DAB8C20
P 4850 4950
F 0 "#PWR012" H 4850 4700 50  0001 C CNN
F 1 "GND" H 4855 4777 50  0000 C CNN
F 2 "" H 4850 4950 50  0001 C CNN
F 3 "" H 4850 4950 50  0001 C CNN
	1    4850 4950
	1    0    0    -1  
$EndComp
Wire Wire Line
	5100 4550 4850 4550
Text Label 4050 4400 2    50   ~ 0
POSITIVE_INPUT
Text Label 1450 4600 2    50   ~ 0
NEGATIVE_OUTPUT
Text Label 1450 4500 2    50   ~ 0
POSITIVE_OUTPUT
$Comp
L power:GND #PWR09
U 1 1 5DAC7A25
P 3250 3400
F 0 "#PWR09" H 3250 3150 50  0001 C CNN
F 1 "GND" H 3255 3227 50  0000 C CNN
F 2 "" H 3250 3400 50  0001 C CNN
F 3 "" H 3250 3400 50  0001 C CNN
	1    3250 3400
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR02
U 1 1 5DAC82DA
P 1050 3200
F 0 "#PWR02" H 1050 2950 50  0001 C CNN
F 1 "GND" H 1055 3027 50  0000 C CNN
F 2 "" H 1050 3200 50  0001 C CNN
F 3 "" H 1050 3200 50  0001 C CNN
	1    1050 3200
	1    0    0    -1  
$EndComp
Wire Wire Line
	1050 3100 1400 3100
$Comp
L power:GND #PWR08
U 1 1 5DAC8EFF
P 3200 2400
F 0 "#PWR08" H 3200 2150 50  0001 C CNN
F 1 "GND" H 3350 2350 50  0000 C CNN
F 2 "" H 3200 2400 50  0001 C CNN
F 3 "" H 3200 2400 50  0001 C CNN
	1    3200 2400
	1    0    0    -1  
$EndComp
Wire Wire Line
	3200 2400 3000 2400
Text Label 3000 2700 0    50   ~ 0
NEGATIVE_INPUT
$Comp
L power:GND #PWR03
U 1 1 5DACAFFD
P 1200 2700
F 0 "#PWR03" H 1200 2450 50  0001 C CNN
F 1 "GND" H 1205 2527 50  0000 C CNN
F 2 "" H 1200 2700 50  0001 C CNN
F 3 "" H 1200 2700 50  0001 C CNN
	1    1200 2700
	1    0    0    -1  
$EndComp
Wire Wire Line
	1400 2700 1200 2700
NoConn ~ 1400 2000
NoConn ~ 1400 1900
NoConn ~ 1400 1800
NoConn ~ 1400 2300
NoConn ~ 1400 2400
NoConn ~ 1400 2500
NoConn ~ 1400 2600
NoConn ~ 3000 1800
NoConn ~ 1400 2900
$Comp
L Connector:Screw_Terminal_01x03 J1
U 1 1 5DAB5A48
P 2150 4100
F 0 "J1" H 2230 4142 50  0000 L CNN
F 1 "Screw_Terminal_01x03" H 2230 4051 50  0000 L CNN
F 2 "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-3-3-5.08_1x03_P5.08mm_Horizontal" H 2150 4100 50  0001 C CNN
F 3 "~" H 2150 4100 50  0001 C CNN
	1    2150 4100
	1    0    0    1   
$EndComp
Wire Wire Line
	1400 3200 1300 3200
$Comp
L Device:R R6
U 1 1 5DAE0AE1
P 4500 4600
F 0 "R6" H 4570 4646 50  0000 L CNN
F 1 "10k" H 4570 4555 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 4430 4600 50  0001 C CNN
F 3 "~" H 4500 4600 50  0001 C CNN
	1    4500 4600
	1    0    0    -1  
$EndComp
$Comp
L Device:R R9
U 1 1 5DAE1364
P 8050 4700
F 0 "R9" H 8120 4746 50  0000 L CNN
F 1 "10k" H 8120 4655 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 7980 4700 50  0001 C CNN
F 3 "~" H 8050 4700 50  0001 C CNN
	1    8050 4700
	1    0    0    -1  
$EndComp
Text Label 7600 4400 2    50   ~ 0
NEGATIVE_INPUT
$Comp
L Device:R R7
U 1 1 5DAB7573
P 7750 4400
F 0 "R7" V 7543 4400 50  0000 C CNN
F 1 "1k" V 7634 4400 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 7680 4400 50  0001 C CNN
F 3 "~" H 7750 4400 50  0001 C CNN
	1    7750 4400
	0    1    1    0   
$EndComp
Wire Wire Line
	7900 4400 8050 4400
Wire Wire Line
	8050 4550 8050 4400
Connection ~ 8050 4400
Wire Wire Line
	8050 4400 8600 4400
Wire Wire Line
	4500 4750 4500 4850
Wire Wire Line
	4500 4450 4500 4400
Wire Wire Line
	4500 4400 4350 4400
Connection ~ 4500 4400
Wire Wire Line
	4500 4400 5100 4400
Wire Wire Line
	1050 3100 1050 3200
$Comp
L power:GND #PWR05
U 1 1 5DC90813
P 1800 1050
F 0 "#PWR05" H 1800 800 50  0001 C CNN
F 1 "GND" H 1805 877 50  0000 C CNN
F 2 "" H 1800 1050 50  0001 C CNN
F 3 "" H 1800 1050 50  0001 C CNN
	1    1800 1050
	1    0    0    -1  
$EndComp
$Comp
L power:PWR_FLAG #FLG01
U 1 1 5DC913A4
P 1800 850
F 0 "#FLG01" H 1800 925 50  0001 C CNN
F 1 "PWR_FLAG" H 1800 1023 50  0000 C CNN
F 2 "" H 1800 850 50  0001 C CNN
F 3 "~" H 1800 850 50  0001 C CNN
	1    1800 850 
	1    0    0    -1  
$EndComp
Wire Wire Line
	1800 1050 1800 850 
Wire Wire Line
	1900 4600 1950 4600
Wire Wire Line
	8050 4850 8050 4900
Wire Wire Line
	8050 4900 8350 4900
Connection ~ 8350 4900
Wire Wire Line
	8350 4900 8350 5000
Wire Wire Line
	4500 4850 4850 4850
Wire Wire Line
	4850 4550 4850 4850
Connection ~ 4850 4850
Wire Wire Line
	4850 4850 4850 4950
$Comp
L power:PWR_FLAG #FLG02
U 1 1 5DD2144A
P 2300 850
F 0 "#FLG02" H 2300 925 50  0001 C CNN
F 1 "PWR_FLAG" H 2300 1023 50  0000 C CNN
F 2 "" H 2300 850 50  0001 C CNN
F 3 "~" H 2300 850 50  0001 C CNN
	1    2300 850 
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR07
U 1 1 5DD22D5E
P 2300 1050
F 0 "#PWR07" H 2300 900 50  0001 C CNN
F 1 "+5V" H 2315 1223 50  0000 C CNN
F 2 "" H 2300 1050 50  0001 C CNN
F 3 "" H 2300 1050 50  0001 C CNN
	1    2300 1050
	-1   0    0    1   
$EndComp
Wire Wire Line
	2300 850  2300 1050
$Comp
L power:+5V #PWR04
U 1 1 5DD568A4
P 1300 3200
F 0 "#PWR04" H 1300 3050 50  0001 C CNN
F 1 "+5V" H 1315 3373 50  0000 C CNN
F 2 "" H 1300 3200 50  0001 C CNN
F 3 "" H 1300 3200 50  0001 C CNN
	1    1300 3200
	1    0    0    -1  
$EndComp
Wire Wire Line
	3250 3400 3250 3100
Wire Wire Line
	3250 3100 3000 3100
$Comp
L power:+3.3V #PWR01
U 1 1 5DD04997
P 1000 2800
F 0 "#PWR01" H 1000 2650 50  0001 C CNN
F 1 "+3.3V" H 1015 2973 50  0000 C CNN
F 2 "" H 1000 2800 50  0001 C CNN
F 3 "" H 1000 2800 50  0001 C CNN
	1    1000 2800
	1    0    0    -1  
$EndComp
Wire Wire Line
	1400 2800 1000 2800
NoConn ~ 3000 2300
NoConn ~ 3000 3200
$Comp
L power:GND #PWR010
U 1 1 5DAD4FD4
P 4300 2250
F 0 "#PWR010" H 4300 2000 50  0001 C CNN
F 1 "GND" H 4305 2077 50  0000 C CNN
F 2 "" H 4300 2250 50  0001 C CNN
F 3 "" H 4300 2250 50  0001 C CNN
	1    4300 2250
	1    0    0    -1  
$EndComp
Wire Wire Line
	3500 2100 3600 2100
$Comp
L Device:R R2
U 1 1 5DAD3A54
P 3350 2100
F 0 "R2" V 3450 2100 50  0000 C CNN
F 1 "1k" V 3350 2100 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 3280 2100 50  0001 C CNN
F 3 "~" H 3350 2100 50  0001 C CNN
	1    3350 2100
	0    1    1    0   
$EndComp
$Comp
L Device:LED D1
U 1 1 5DACE2A0
P 3750 2100
F 0 "D1" H 3850 2200 50  0000 C CNN
F 1 "LED" H 3700 2200 50  0000 C CNN
F 2 "LED_THT:LED_D3.0mm_Horizontal_O3.81mm_Z2.0mm" H 3750 2100 50  0001 C CNN
F 3 "~" H 3750 2100 50  0001 C CNN
	1    3750 2100
	-1   0    0    1   
$EndComp
Text Label 3000 2600 0    50   ~ 0
POSITIVE_INPUT
Wire Wire Line
	3200 2100 3000 2100
NoConn ~ 1400 2200
$Comp
L Switch:SW_Push SW1
U 1 1 5DD33382
P 1450 6650
F 0 "SW1" H 1450 6935 50  0000 C CNN
F 1 "SW_Push" H 1450 6844 50  0000 C CNN
F 2 "Button_Switch:SW_Tactile_SKHH_Angled" H 1450 6850 50  0001 C CNN
F 3 "~" H 1450 6850 50  0001 C CNN
	1    1450 6650
	0    -1   -1   0   
$EndComp
$Comp
L Device:C C1
U 1 1 5DD346D5
P 1850 6500
F 0 "C1" H 1965 6546 50  0000 L CNN
F 1 "100nF" H 1965 6455 50  0000 L CNN
F 2 "Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm" H 1888 6350 50  0001 C CNN
F 3 "~" H 1850 6500 50  0001 C CNN
	1    1850 6500
	1    0    0    -1  
$EndComp
$Comp
L Device:R R1
U 1 1 5DD38008
P 1850 6950
F 0 "R1" H 1920 6996 50  0000 L CNN
F 1 "1k" H 1920 6905 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 1780 6950 50  0001 C CNN
F 3 "~" H 1850 6950 50  0001 C CNN
	1    1850 6950
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR06
U 1 1 5DD38A19
P 1850 7200
F 0 "#PWR06" H 1850 6950 50  0001 C CNN
F 1 "GND" H 1855 7027 50  0000 C CNN
F 2 "" H 1850 7200 50  0001 C CNN
F 3 "" H 1850 7200 50  0001 C CNN
	1    1850 7200
	1    0    0    -1  
$EndComp
Wire Wire Line
	1850 7100 1850 7150
Wire Wire Line
	1850 6650 1850 6800
Text Label 1350 6350 2    50   ~ 0
USER_BUTTON
Text Label 3000 1900 0    50   ~ 0
USER_BUTTON
NoConn ~ 3000 2200
NoConn ~ 3000 2800
Wire Wire Line
	3900 2100 4300 2100
Text Label 5850 1400 2    50   ~ 0
LIVE
$Comp
L power:+5V #PWR013
U 1 1 5E4D9FEA
P 5800 1850
F 0 "#PWR013" H 5800 1700 50  0001 C CNN
F 1 "+5V" H 5815 2023 50  0000 C CNN
F 2 "" H 5800 1850 50  0001 C CNN
F 3 "" H 5800 1850 50  0001 C CNN
	1    5800 1850
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR014
U 1 1 5E4DAA3D
P 5800 1950
F 0 "#PWR014" H 5800 1700 50  0001 C CNN
F 1 "GND" H 5805 1777 50  0000 C CNN
F 2 "" H 5800 1950 50  0001 C CNN
F 3 "" H 5800 1950 50  0001 C CNN
	1    5800 1950
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x02_Female J3
U 1 1 5E4DB44A
P 6100 1850
F 0 "J3" H 6128 1826 50  0000 L CNN
F 1 "Conn_01x02_Female" H 6128 1735 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" H 6100 1850 50  0001 C CNN
F 3 "~" H 6100 1850 50  0001 C CNN
	1    6100 1850
	1    0    0    -1  
$EndComp
Wire Wire Line
	5900 1850 5800 1850
Wire Wire Line
	5800 1950 5900 1950
Wire Wire Line
	5950 1400 5850 1400
Text Label 5850 1500 2    50   ~ 0
NEUTRAL
Wire Wire Line
	5850 1500 5950 1500
Wire Wire Line
	1350 6350 1450 6350
Wire Wire Line
	1450 6450 1450 6350
Connection ~ 1450 6350
Wire Wire Line
	1450 6350 1850 6350
Wire Wire Line
	1450 6850 1450 7150
Wire Wire Line
	1450 7150 1850 7150
Connection ~ 1850 7150
Wire Wire Line
	1850 7150 1850 7200
$Comp
L Device:R R5
U 1 1 5E52FD60
P 4500 4150
F 0 "R5" H 4570 4196 50  0000 L CNN
F 1 "10k" H 4570 4105 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 4430 4150 50  0001 C CNN
F 3 "~" H 4500 4150 50  0001 C CNN
	1    4500 4150
	1    0    0    -1  
$EndComp
Wire Wire Line
	4500 4300 4500 4400
$Comp
L power:+3.3V #PWR011
U 1 1 5E534F3B
P 4500 3850
F 0 "#PWR011" H 4500 3700 50  0001 C CNN
F 1 "+3.3V" H 4515 4023 50  0000 C CNN
F 2 "" H 4500 3850 50  0001 C CNN
F 3 "" H 4500 3850 50  0001 C CNN
	1    4500 3850
	1    0    0    -1  
$EndComp
Wire Wire Line
	4500 3850 4500 4000
$Comp
L Device:R R8
U 1 1 5E545E1A
P 8050 4100
F 0 "R8" H 8120 4146 50  0000 L CNN
F 1 "10k" H 8120 4055 50  0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 7980 4100 50  0001 C CNN
F 3 "~" H 8050 4100 50  0001 C CNN
	1    8050 4100
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR015
U 1 1 5E546303
P 8050 3850
F 0 "#PWR015" H 8050 3700 50  0001 C CNN
F 1 "+3.3V" H 8065 4023 50  0000 C CNN
F 2 "" H 8050 3850 50  0001 C CNN
F 3 "" H 8050 3850 50  0001 C CNN
	1    8050 3850
	1    0    0    -1  
$EndComp
Wire Wire Line
	8050 4250 8050 4400
Wire Wire Line
	8050 3850 8050 3950
Text Notes 7250 4000 0    50   ~ 0
Select pull-up or \npull-down resistor
Text Notes 3700 4000 0    50   ~ 0
Select pull-up or \npull-down resistor
Wire Notes Line
	7300 850  5350 850 
Text Notes 6000 1000 0    50   ~ 0
220VAC/5VDC circuit
NoConn ~ 3000 2900
NoConn ~ 3000 3000
NoConn ~ 1400 2100
NoConn ~ 1400 3000
Text Label 1900 4600 2    50   ~ 0
LIVE
Text Label 1950 4700 2    50   ~ 0
NEUTRAL
Wire Wire Line
	1800 4200 1950 4200
$Comp
L ESP8266:NodeMCU_1.0_(ESP-12E) U1
U 1 1 5DA8ED28
P 2200 2500
F 0 "U1" H 2200 3587 60  0000 C CNN
F 1 "NodeMCU_1.0_(ESP-12E)" H 2200 3481 60  0000 C CNN
F 2 "nodemcu:NodeMCU" H 1600 1650 60  0001 C CNN
F 3 "" H 1600 1650 60  0000 C CNN
	1    2200 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	4300 2100 4300 2250
NoConn ~ 3000 2500
$Comp
L power:+5V #PWR0101
U 1 1 5E68380E
P 5800 2400
F 0 "#PWR0101" H 5800 2250 50  0001 C CNN
F 1 "+5V" H 5815 2573 50  0000 C CNN
F 2 "" H 5800 2400 50  0001 C CNN
F 3 "" H 5800 2400 50  0001 C CNN
	1    5800 2400
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0102
U 1 1 5E683814
P 5800 2500
F 0 "#PWR0102" H 5800 2250 50  0001 C CNN
F 1 "GND" H 5805 2327 50  0000 C CNN
F 2 "" H 5800 2500 50  0001 C CNN
F 3 "" H 5800 2500 50  0001 C CNN
	1    5800 2500
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x02_Female J4
U 1 1 5E68381A
P 6100 2400
F 0 "J4" H 6128 2376 50  0000 L CNN
F 1 "Conn_01x02_Female" H 6128 2285 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" H 6100 2400 50  0001 C CNN
F 3 "~" H 6100 2400 50  0001 C CNN
	1    6100 2400
	1    0    0    -1  
$EndComp
Wire Wire Line
	5900 2400 5800 2400
Wire Wire Line
	5800 2500 5900 2500
Wire Notes Line
	5350 2800 7300 2800
Wire Notes Line
	7300 850  7300 2800
Wire Notes Line
	5350 850  5350 2800
$Comp
L Connector:Conn_01x01_Female J7
U 1 1 5E686627
P 6150 1500
F 0 "J7" H 6178 1526 50  0000 L CNN
F 1 "Conn_01x01_Female" H 6178 1435 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 6150 1500 50  0001 C CNN
F 3 "~" H 6150 1500 50  0001 C CNN
	1    6150 1500
	1    0    0    -1  
$EndComp
$Comp
L Connector:Conn_01x01_Female J6
U 1 1 5E686883
P 6150 1400
F 0 "J6" H 6178 1426 50  0000 L CNN
F 1 "Conn_01x01_Female" H 6178 1335 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 6150 1400 50  0001 C CNN
F 3 "~" H 6150 1400 50  0001 C CNN
	1    6150 1400
	1    0    0    -1  
$EndComp
Text Label 5950 1250 2    50   ~ 0
EARTH
$Comp
L Connector:Conn_01x01_Female J5
U 1 1 5E686E3E
P 6150 1250
F 0 "J5" H 6178 1276 50  0000 L CNN
F 1 "Conn_01x01_Female" H 6178 1185 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x01_P2.54mm_Vertical" H 6150 1250 50  0001 C CNN
F 3 "~" H 6150 1250 50  0001 C CNN
	1    6150 1250
	1    0    0    -1  
$EndComp
Text Label 1750 4000 2    50   ~ 0
EARTH
Wire Wire Line
	1950 4000 1750 4000
Wire Wire Line
	1450 4600 1450 4500
Wire Wire Line
	1450 4500 1950 4500
NoConn ~ 3000 2000
$EndSCHEMATC
