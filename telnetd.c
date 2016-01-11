/*
 * Copyright (c) 2003, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack
 *
 * $Id: telnetd.c,v 1.2 2006/06/07 09:43:54 adam Exp $
 *
 */

#include "uip.h"
#include "telnetd.h"

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
 *  |128. < Connection established, idle >
 *  | -|--
 *  |129. < Connection established, tx data sent, waiting ack >
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

struct txDataType {
	u8_t data[TCPTXBUFSIZE];
	u8_t length;
} txData;

void telnetd_init(void) {
	uip_listen(HTONS(23));
	memb_init(&linemem);
	shell_init();
}

void connClosed() {
	telnetState = 0;
}

void telnetd_appcall(void) {
	if(uip_connected()) {
		// new connection
		// telnet setup
		telnetState = 0;
		txData.data = { TELNET_IAC, TELNET_WILL, TELNET_MODE_LINEMODE };
		txData.length = 3;
		uip_send( txData.data, txData.length );
	}
	
	if(uip_closed() ||
		 uip_aborted() ||
		 uip_timedout()) {
		connClosed();
	}
	
	if(uip_acked()) {
		if( telnetState == 129 ) {
			telnetState = 128; // tcp ack
		}
	}
	
	if(uip_newdata()) {
		u8_t rxDataByte, rxDataLength = uip_datalen();
		if( telnetState & 128 ) {
			// connection is good, pass recieved data to UART
			UART0PushSend( uip_appdata, uip_datalen() );
		}
		else {
			for( rxDataByte = 0; rxDataByte < rxDataLength && telnetState < rxExpectedCount; ) {
				if( uip_appdata[rxDataByte++] != rxExpected[telnetState] ) {
					// unexpected answer, client does not support tis mode
					telnetState = 0;
					uip_close();
					connClosed();
					return;
				}
			}
			if( telnetState == rxExpectedCount ) {
				// telnet setup completed
				telnetState = 128; // state: idle
				return;
			}
			switch( telnetState ) {
				case 0:
					txData.data = { TELNET_IAC, TELNET_WILL, TELNET_MODE_LINEMODE };
					txData.length = 3;
					uip_send( txData.data, txData.length );
					break;
				case 3:
					txData.data = { TELNET_IAC, TELNET_SB, TELNET_MODE_LINEMODE, TELNET_MODE_LINEMODE_EDIT, 0, TELNET_IAC, TELNET_SE };
					txData.length = 7;
					uip_send( txData.data, txData.length );
					break;
				case 10:
					txData.data = { TELNET_IAC, TELNET_SB, TELNET_MODE_LINEMODE, TELNET_MODE_LINEMODE_TRAPSIG, 0, TELNET_IAC, TELNET_SE };
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
