/****************************************************************************************************

PowerDueFirware - SD Configuration

This firmware is a port from:
"Version 0.1
18 November 2012
Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com
Licence: GPL"

To work with the PowerDue
-----------------------------------------------------------------------------------------------------
****************************************************************************************************/

#ifndef ARDUINO_DUE_SD_HSMCI_CONFIGURATION_H
#define ARDUINO_DUE_SD_HSMCI_CONFIGURATION_H
#ifdef __SAM3X8E__ //only __SAM3X8E__

#define NAME "PowerDue firmware"
#define VERSION "0.0.1"
#define DATE "2016-01-25"
#define AUTHORS "JoaoDiogoFalcao @ CMU"


// String lengths

#define STRING_LENGTH 1024
#define SHORT_STRING_LENGTH 40

#define FILENAME_LENGTH 100
#define GCODE_REPLY_LENGTH 2048

#define LIST_SEPARATOR ':'						// Lists in G Codes
#define FILE_LIST_SEPARATOR ','					// Put this between file names when listing them
#define FILE_LIST_BRACKET '"'					// Put these round file names when listing them

#define LONG_TIME 300.0 // Seconds

#define EOF_STRING "<!-- **EoF** -->"           // For HTML uploads
#endif //only __SAM3X8E__
#endif
