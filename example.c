/**
 * @file
 * Example of usage Provisioning Module.
 *
 * @author Imagination Technologies
 *
 * @copyright Copyright (c) 2016, Imagination Technologies Limited and/or its affiliated group
 * companies and/or licensors.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "sys/clock.h"
#include "pic32_gpio.h"

#include "provision_library.h"
#include "provision_crypto.h"
#include "diffie_hellman_keys_exchanger.h"


#include "provision_communication.h"

#define LED1_PORT   B
#define LED1_PIN    1
#define LED2_PORT   B
#define LED2_PIN    2

PROCESS(bootProcess, "Boot process");
PROCESS(exampleProcess, "Example process");
AUTOSTART_PROCESSES(&bootProcess);

void ConfigureLEDs()
{
  GPIO_CONFIGURE_AS_DIGITAL(LED1_PORT, LED1_PIN);
  GPIO_CONFIGURE_AS_OUTPUT(LED1_PORT, LED1_PIN);
  GPIO_CLR(LED1_PORT, LED1_PIN);
  GPIO_CONFIGURE_AS_DIGITAL(LED2_PORT, LED2_PIN);
  GPIO_CONFIGURE_AS_OUTPUT(LED2_PORT, LED2_PIN);
  GPIO_CLR(LED2_PORT, LED2_PIN);
}

void ConfigCallback(Provision_Configuration* config) {
  printf("Config callback\n");
}

PROCESS_THREAD(bootProcess, ev, data)
{
  PROCESS_BEGIN();
  provision_boot(&exampleProcess, ConfigCallback);
  PROCESS_END();
}

PROCESS_THREAD(exampleProcess, ev, data)
{
  PROCESS_BEGIN();
  PROCESS_PAUSE();
  ConfigureLEDs();
  printf("EXAMPLE APP IS RUNNING\n");
  static int led = 0;
  while(1) {
    static struct etimer et;
    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    led ++;
    if (led % 2 == 0) {
      GPIO_SET(LED1_PORT, LED1_PIN);
      GPIO_CLR(LED2_PORT, LED2_PIN);

    } else {
      GPIO_CLR(LED1_PORT, LED1_PIN);
      GPIO_SET(LED2_PORT, LED2_PIN);
    }
  }

  PROCESS_END();
}
