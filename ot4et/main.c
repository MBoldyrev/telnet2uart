# 1 "../main.c"
#include "uip/uip.h"
#include "uip/uip_arp.h"
#include "uip/httpd.h"
#include "uip/timer.h"
#include "lpc17xx_emac.h"
#include "lpc_phy.h"
#include "uart.h"


int
main(void)
{
  int i;
  uip_ipaddr_t ipaddr;
  struct timer periodic_timer;

  timer_set(&periodic_timer, CLOCK_SECOND / 2);

  EMAC_Init();
  lpc_phy_init();
  uip_init();

  uip_ipaddr(ipaddr, 192,168,0,55);
  uip_sethostaddr(ipaddr);

  UART0Init();
  uip_listen(HTONS(23));

  while(1) {
    uip_len = EMAC_ReadPacket( uip_buf );
    if(uip_len > 0) {
      uip_input();



      if(uip_len > 0) {
        EMAC_SendPacket( uip_buf, uip_len );
      }
    } else if(timer_expired(&periodic_timer)) {
      timer_reset(&periodic_timer);
      for(i = 0; i < UIP_CONNS; i++) {
        uip_periodic(i);



        if(uip_len > 0) {
          EMAC_SendPacket( uip_buf, uip_len );
        }
      }
    }
  }
  return 0;
}
