#ifndef BOARDS_H
#define BOARDS_H

#define BOARD_UNKNOWN -1

#define BOARD_RADDS			402  // RADDS
#define BOARD_RAMPS_FD_V1	403  // RAMPS-FD V1 WARNING:If you have a RAMPS_FD_V1 on this guide http://forums.reprap.org/read.php?219,424146,507810, you must use RAMPS_FD_V2 configuration 
#define BOARD_RAMPS_FD_V2	404  // RAMPS-FD V2
#define BOARD_RAMPS4DUE     433  // RAMPS4DUE with AndrewBCN's RAMPS mods (http://forums.reprap.org/read.php?219,479626,page=1)
#define BOARD_AMBIT         303  // AMBIT (Rambo32 SAM3X8E + DRV8825)
#define BOARD_FEYNMANRAMBO         304  // FEYNMAN (SAMG55 + 4 TMC drivers)

#define MB(board) (MOTHERBOARD==BOARD_##board)

#endif //__BOARDS_H
