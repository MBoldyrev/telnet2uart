#include "uip/uip.h"
#include "uip/uip_arp.h"
#include "uip/httpd.h"
#include "uip/timer.h"
#include "network-device.h"
#include "uart.h"

/*---------------------------------------------------------------------------*/
int
main(void)
{
  int i;
  uip_ipaddr_t ipaddr;
  struct timer periodic_timer;
  
  timer_set(&periodic_timer, CLOCK_SECOND / 2);
  
  network_device_init();
  uip_init();

  uip_ipaddr(ipaddr, 192,168,0,55);
  uip_sethostaddr(ipaddr);

  UART0Init();
  uip_listen(HTONS(23));
  
  while(1) {
    uip_len = network_device_read();
    if(uip_len > 0) {
      uip_input();
      /* If the above function invocation resulted in data that
 *          should be sent out on the network, the global variable
 *                   uip_len is set to a value > 0. */
      if(uip_len > 0) {
        network_device_send();
      }
    } else if(timer_expired(&periodic_timer)) {
      timer_reset(&periodic_timer);
      for(i = 0; i < UIP_CONNS; i++) {
        uip_periodic(i);
        /* If the above function invocation resulted in data that
 *            should be sent out on the network, the global variable
 *                       uip_len is set to a value > 0. */
        if(uip_len > 0) {
          network_device_send();
        }
      }
    }
  }
  return 0;
}
