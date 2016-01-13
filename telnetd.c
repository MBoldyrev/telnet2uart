#include "uip/uip.h"
#include "telnetd.h"
#include "uart.h"

#define ISO_nl       0x0a
#define ISO_cr       0x0d

#define STATE_NORMAL 	0
#define STATE_IAC    	1
#define STATE_WILL   	2
#define STATE_WONT   	3
#define STATE_DO     	4
#define STATE_DONT   	5
#define STATE_CLOSE  	6

#define TELNET_SB		4
#define TELNET_MODE_LINEMODE			34
#define TELNET_MODE_LINEMODE_EDIT		1
#define TELNET_MODE_LINEMODE_TRAPSIG	2

#define TELNET_SE		240
#define TELNET_WILL		251
#define TELNET_WONT		252
#define TELNET_DO		253
#define TELNET_DONT		254
#define TELNET_IAC		255

/*    ,--------------------.
 *   /    telnetState      |
 *  /   ,------------------'
 *  |  /  Action
 *  | -|--
 *  |0.|  < Send >
 *  |  |  IAC WILL LINEMODE
 *  |  |  255 251  34
 *  | -|--
 *  |0.|  < Wait for >
 *  |  |  IAC DO  LINEMODE
 *  |  |  255 253 34
 *  |  |  [0] [1] [2]
 *  | -|--
 *  |3.|  < Send >
 *  |  |  IAC SB  LINEMODE EDIT 0 IAC SE
 *  |  |  255 250 34       1    0 255 240
 *  | -|--
 *  |3.|  < Wait for >
 *  |  |  IAC SB  LINEMODE EDIT 0|ACK IAC SE
 *  |  |  255 250 34       1    4     255 240
 *  |  |  [3] [4] [5]      [6]  [7]   [8] [9]
 *  | -|-- 
 *  |10.  < Send >
 *  |  |  IAC SB  LINEMODE TRAPSIG 0 IAC SE
 *  |  |  255 250 34       2       0 255 240
 *  | -|--
 *  |10.  < Wait for >
 *  |  |  IAC SB  LINEMODE TRAPSIG 0|ACK IAC SE
 *  |  |  255 250 34       2       4     255 240
 *  |  |  [10][11][12]     [13]    [14]  [15][16]
 *  | -|--
 *  |17.  < Telnet setup complete >
 *  | -|--
 *  |128. < Connection established >
 *  |  |
 *   \/ 
 * 
 */

const u8_t rxExpectedCount = 17, rxExpected[] = {
	255, 253, 34,
	255, 250, 34, 1, 4, 255, 240,
	255, 250, 34, 2, 4, 255, 240
};
u8_t telnetState = 0, rxExpectedByte = 0;

void telnetd_appcall(void) {
	if(uip_connected()) {
		// new connection
		// telnet setup
		telnetState = 0;
		txData.data[0] = TELNET_IAC;
		txData.data[1] = TELNET_WILL;
		txData.data[2] = TELNET_MODE_LINEMODE;
		txData.length = 3;
		uip_send( txData.data, txData.length );
	}
	
	if(uip_closed() ||
		uip_aborted() ||
		uip_timedout()) {
		telnetState = 0;
		txData.length = 0;
	}
	
	if(uip_acked()) {
		txData.length = 0;
	}
	
	if(uip_newdata()) {
		if( telnetState & 128 ) {
			// connection is good, pass recieved data to UART
			UART0PushSend( uip_appdata, uip_datalen() );
		}
		else {
			u8_t rxDataByte, rxDataLength = uip_datalen();
			for( rxDataByte = 0; rxDataByte < rxDataLength && telnetState < rxExpectedCount; ) {
				if( ((uint8_t*)uip_appdata)[rxDataByte++] != rxExpected[telnetState] ) {
					// unexpected answer, client does not support tis mode
					if( telnetState < 17 )
						telnetState = 10;
					if( telnetState < 10 )
						telnetState = 3;
					if( telnetState < 3 )
						telnetState = 0;
					uip_close();
					connClosed();
					return;
				}
			}
			if( telnetState == rxExpectedCount ) {
				// telnet setup completed
				telnetState = 128;
				return;
			}
			switch( telnetState ) {
				case 0:
					txData.data[0] = TELNET_IAC;
					txData.data[1] = TELNET_WILL;
					txData.data[2] = TELNET_MODE_LINEMODE;
					txData.length = 3;
					uip_send( txData.data, txData.length );
					break;
				case 3:
					txData.data[0] = TELNET_IAC;
					txData.data[1] = TELNET_SB;
					txData.data[2] = TELNET_MODE_LINEMODE;
					txData.data[3] = TELNET_MODE_LINEMODE_EDIT;
					txData.data[4] = 0;
					txData.data[5] = TELNET_IAC;
					txData.data[6] = TELNET_SE;
					txData.length = 7;
					uip_send( txData.data, txData.length );
					break;
				case 10:
					txData.data[0] = TELNET_IAC;
					txData.data[1] = TELNET_SB;
					txData.data[2] = TELNET_MODE_LINEMODE;
					txData.data[3] = TELNET_MODE_LINEMODE_TRAPSIG;
					txData.data[4] = 0;
					txData.data[5] = TELNET_IAC;
					txData.data[6] = TELNET_SE;
					txData.length = 7;
					uip_send( txData.data, txData.length );
					break;
			}
		}
	}
	
	if( uip_rexmit() ) {
		uip_send( txData.data, txData.length );
	}
}
