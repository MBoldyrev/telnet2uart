#include "uip/uip.h"
#include "telnetd.h"
#include "uart.h"

#define TELNET_SB 4
#define TELNET_MODE_LINEMODE 34
#define TELNET_MODE_LINEMODE_EDIT 1
#define TELNET_MODE_LINEMODE_TRAPSIG 2

#define TELNET_SE 240
#define TELNET_WILL 251
#define TELNET_WONT 252
#define TELNET_DO 253
#define TELNET_DONT 254
#define TELNET_IAC 255

const u8_t rxExpectedCount = 17, rxExpected[] = {
  255, 253, 34,
  255, 250, 34, 1, 4, 255, 240,
  255, 250, 34, 2, 4, 255, 240
};
u8_t telnetState = 0, rxExpectedByte = 0;

void telnetd_appcall(void) {

  if(uip_connected()) {
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
      UART0PushSend( uip_appdata, uip_datalen() );
    }
    else {
      u8_t rxDataByte, rxDataLength = uip_datalen();
      for( rxDataByte = 0; rxDataByte < rxDataLength && telnetState < rxExpectedCount; ) {
        if( ((uint8_t*)uip_appdata)[rxDataByte++] != rxExpected[telnetState] ) {
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
