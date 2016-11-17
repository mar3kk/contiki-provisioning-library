/**
 * @file
 * Provisioning library main module.
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
#include <string.h>
#include "contiki-net.h"
#include "provision_library.h"
#include "provision_communication.h"
#include "provision_crypto.h"
#include "provision_config.h"
#include "lib/sensors.h"
#include "button-sensor.h"
#include "helpers.h"
#include "sys/process.h"
#include "bigint.h"

//-------- Forward declarations ------
static void cleanupAndBootMasterProcess();
static size_t strnlen(const char* s, size_t len);
static void finalizeConfiguration();
static void busyLEDControl(int state);

//-------- Process declarations ------
PROCESS(provisionProcessNoConfig, "Provision procedure NO CONFIG");
PROCESS(provisionProcessConfigured, "Provision procedure WITH CONFIG");

//-------- Defines, typedefs and variables ------
#define LED1_PORT   B
#define LED1_PIN    1
#define LED2_PORT   B
#define LED2_PIN    2

#define PROVISION_CONFIG_FLAG_SERVER  0x1
#define PROVISION_CONFIG_FLAG_NETWORK 0x2

typedef enum {
    Provision_BootMode_MODE_NO_CONFIG,     //device is fresh programmed, or config was erased
    Provision_BootMode_MODE_HAS_CONFIG     //device is provisioned
} Provision_BootMode;

static Provision_BootMode _BootMode = Provision_BootMode_MODE_NO_CONFIG;
static ProvisionConfigCallback _ConfigCallback = NULL;
static int _WantConnection = 1;
static bool _isConncted = false;
static int _ConfigFlags = 0;
static struct process* _ProcessToRun;

//this two fields are set if configuration of server came before we have ready shared key to decrypt it
static uint8_t* _DelayedServerConfigData = NULL;
static uint8_t _DelayedServerConfigDataLen = 0;
static uint8_t* _DelayedNetworkConfigData = NULL;
static uint8_t _DelayedNetworkConfigDataLen = 0;

//-------- Command handlers ------
static void Provision_HandlerNetworkCommandType_HIGHLIGHT(Provision_NetworkCommand cmd) {
#ifdef PROVISION_DEBUG
    printf("Provision: Request for highlight.\n");
#endif
  GPIO_SET(LED2_PORT, LED2_PIN);
}

static void Provision_HandlerNetworkCommandType_DISABLEHIGHLIGHT(Provision_NetworkCommand cmd) {
#ifdef PROVISION_DEBUG
    printf("Provision: Request to end highlight.\n");
#endif
  GPIO_CLR(LED2_PORT, LED2_PIN);
}

uint8_t buffer;

static void Provision_HandlerNetworkCommandType_KEEP_ALIVE(Provision_NetworkCommand cmd) {
#ifdef PROVISION_DEBUG
    printf("Provision: Keep alive, ping. We do pong.\n");
#endif
  buffer = Provision_NetworkCommandType_KEEP_ALIVE;
  provision_sendData(&buffer, sizeof(buffer));
}

static void Provision_HandlerNetworkCommandType_SERVER_CONFIG(Provision_NetworkCommand cmd) {
#ifdef PROVISION_DEBUG
    printf("Provision: We got server config.\n");
#endif
  _DelayedServerConfigData = malloc(cmd.dataSize);
  _DelayedServerConfigDataLen = cmd.dataSize;
  memcpy(_DelayedServerConfigData, cmd.dataPtr, cmd.dataSize);
  _ConfigFlags |= PROVISION_CONFIG_FLAG_SERVER;
}

static void Provision_HandlerNetworkCommandType_NETWORK_CONFIG(Provision_NetworkCommand cmd) {
#ifdef PROVISION_DEBUG
    printf("Provision: We got network config.\n");
#endif
  _DelayedNetworkConfigData = malloc(cmd.dataSize);
  _DelayedNetworkConfigDataLen = cmd.dataSize;
  memcpy(_DelayedNetworkConfigData, cmd.dataPtr, cmd.dataSize);
  _ConfigFlags |= PROVISION_CONFIG_FLAG_NETWORK;
}

static void Provision_HandlerNetworkCommandType_PUBLIC_KEY(Provision_NetworkCommand cmd) {
  #ifdef PROVISION_DEBUG
      printf("Provision: Public key from server.\n");
      PRINT_BYTES(cmd.dataPtr, cmd.dataSize);
  #endif
  provision_setServerPublicKey(cmd.dataPtr, cmd.dataSize);

  //respond with our public key
  uint8_t len;
  uint8_t* publicKey = provision_getPublicKey(&len);

#ifdef PROVISION_DEBUG
  printf("Provision: Sending public key\n");
  PRINT_BYTES(publicKey, len);
#endif

  uint8_t command[2 + len];
  command[0] = Provision_NetworkCommandType_PUBLIC_KEY;
  command[1] = len;
  memcpy(&command[2], publicKey, len);
  provision_sendData(command, 2 + len);
}

static void Provision_CommunicationStateListener(Provision_CommunicationState state) {
#ifdef PROVISION_DEBUG
    printf("Provision: Connection state:%s\n", state == Provision_CommunicationState_CONNECTED ? "Connected" : "Disconnected");
#endif

  if (state == Provision_CommunicationState_DISCONNECTED) {
    _WantConnection = 1;
    _isConncted = false;
    GPIO_CLR(LED2_PORT, LED2_PIN);  //if we were highlighted, disable it

  } else {
    _WantConnection = 0;
    _isConncted = 1;
  }
}

static Provision_NetworkCommandHandlerBind _ProvisionCommandHandlers[] = {
  {
    .command = Provision_NetworkCommandType_ENABLE_HIGHLIGHT,
    .dataIsExpected = false,
    .handler = &Provision_HandlerNetworkCommandType_HIGHLIGHT
  },
  {
    .command = Provision_NetworkCommandType_DISABLE_HIGHLIGHT,
    .dataIsExpected = false,
    .handler = &Provision_HandlerNetworkCommandType_DISABLEHIGHLIGHT
  },
  {
    .command = Provision_NetworkCommandType_KEEP_ALIVE,
    .dataIsExpected = false,
    .handler = &Provision_HandlerNetworkCommandType_KEEP_ALIVE
  },
  {
    .command = Provision_NetworkCommandType_PUBLIC_KEY,
    .dataIsExpected = true,
    .handler = &Provision_HandlerNetworkCommandType_PUBLIC_KEY
  },
  {
    .command = Provision_NetworkCommandType_SERVER_CONFIG,
    .dataIsExpected = true,
    .handler = &Provision_HandlerNetworkCommandType_SERVER_CONFIG
  },
  {
    .command = Provision_NetworkCommandType_NETWORK_CONFIG,
    .dataIsExpected = true,
    .handler = &Provision_HandlerNetworkCommandType_NETWORK_CONFIG
  },

  //this is terminator
  {
    .command = Provision_NetworkCommandType_NONE,
    .dataIsExpected = false,
    .handler = NULL
  },
};

PROCESS_THREAD(provisionProcessConfigured, ev, data) {
  PROCESS_BEGIN();
  static int _WaitTime;
  _WaitTime = PROVISION_WAIT_TIME_TO_BOOT;
  static struct etimer et;
  bool runMasterProcess = true;
#ifdef PROVISION_DEBUG
  printf("Provision: Waiting %d ms, press any key to reset config or just wait to proceed.\n", _WaitTime);
#endif
  GPIO_SET(LED1_PORT, LED1_PIN);

  etimer_set(&et, (_WaitTime * CLOCK_SECOND) / 1000);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et) || (ev == sensors_event));

  if (data == &button_sensor)	{
#ifdef PROVISION_DEBUG
      printf("Provision: Requested reset of configuration! Going into full provisioning mode\n");
#endif
      provision_clearConfig();
      process_start(&provisionProcessNoConfig, NULL);
      runMasterProcess = false;
  }

  GPIO_CLR(LED1_PORT, LED1_PIN);
  if (runMasterProcess == true) {
      provision_loadConfig();
      cleanupAndBootMasterProcess();
  }

  PROCESS_END();
}

PROCESS_THREAD(provisionProcessNoConfig, ev, data)
{
  PROCESS_BEGIN();
#ifdef PROVISION_DEBUG
  printf("Provision: Start of provision, provision daemon is at:%s\n", PROVISION_DAEMON);
#endif

  GPIO_SET(LED1_PORT, LED1_PIN);
  bi_setBusyLEDControl(busyLEDControl);
  provision_preparePublicKey();

  //resolve destination
  static uip_ipaddr_t destAddr;
  while(1) {
    resolv_query(PROVISION_DAEMON);
    PROCESS_WAIT_EVENT();
    if(ev == resolv_event_found) {
      uip_ipaddr_t* tmp;
      resolv_status_t status = resolv_lookup(PROVISION_DAEMON, &tmp);
      
      if (status == RESOLV_STATUS_CACHED && tmp != NULL) {
        memcpy(&destAddr, tmp, sizeof(uip_ipaddr_t));
#ifdef PROVISION_DEBUG
        printf("Provision: Found provision daemon at:");
        PRINT6ADDR(&destAddr);
        printf("\n");
#endif
        break;
      }
    }
  }

  provision_setDefaultRoute(&destAddr);
  //configure communication
  provision_setCommandHandlers(&_ProvisionCommandHandlers[0], &Provision_CommunicationStateListener);

  static int ledTmp = 0;
  static int exitCondition = PROVISION_CONFIG_FLAG_SERVER | PROVISION_CONFIG_FLAG_NETWORK;

  while( (_ConfigFlags & exitCondition) != exitCondition) {
    static struct etimer et;
    etimer_set(&et, ((_isConncted ? 300 : 1000) * CLOCK_SECOND) / 1000);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et) );

    if (_WantConnection == 1) {
      provision_openIPConnection(NULL, PROVISION_DAEMON_PORT, &destAddr);
      _ConfigFlags = 0;
      FREE_AND_NULL(_DelayedServerConfigData);
      _DelayedServerConfigDataLen = 0;
      FREE_AND_NULL(_DelayedNetworkConfigData);
      _DelayedNetworkConfigData = 0;

      _WantConnection = 0;
    }

    //Blink leds
    ledTmp = !ledTmp;
    if (ledTmp) {
      GPIO_SET(LED1_PORT, LED1_PIN);
    } else {
      GPIO_CLR(LED1_PORT, LED1_PIN);
    }

    PROCESS_PAUSE();
  }
#ifdef PROVISION_DEBUG
  printf("Provision: Communication done, processing provision data.\n");
#endif

  provision_closeConnection();
  provision_finalizeKeyExchange();
  finalizeConfiguration();

  //we have configuration now stored, proceed to master process.
  provision_verifyConfig();
  provision_saveConfig();
  cleanupAndBootMasterProcess();

  PROCESS_END();
}

static void busyLEDControl(int state) {
  if (state & 1) {
    GPIO_SET(LED2_PORT, LED2_PIN);
  } else {
    GPIO_CLR(LED2_PORT, LED2_PIN);
  }
}

static char* strndup(uint8_t* src, int maxLen ) {
  int len = strnlen(src, maxLen);
  char* result = malloc(len + 1);
  memset(result, 0, len + 1);
  memcpy(result, src, len);
  result[len] = 0;
}

static void finalizeConfiguration() {

  if (provision_readyForDecrypt() == false || _DelayedServerConfigData == NULL || _DelayedNetworkConfigData == NULL) {
#ifdef PROVISION_DEBUG
    printf("Provision: can't finalize configuration, sharedKey: %d, server config:%d, network config:%d\n",
      provision_readyForDecrypt(), _DelayedServerConfigData != NULL, _DelayedNetworkConfigData != NULL);
#endif
    return;
  }

#ifdef PROVISION_DEBUG
  printf("Provision: finalizing configuration.\n");
#endif
  //Network configuration
  provision_decodeBytes(_DelayedNetworkConfigData, _DelayedNetworkConfigDataLen);
  Provision_NewtworkConfig* netCfg = (Provision_NewtworkConfig*)_DelayedNetworkConfigData;
  _ProvisionConfiguration.defaultRouteUri = strndup(netCfg->defaultRouteUri, 100);
  _ProvisionConfiguration.dnsServer = strndup(netCfg->dnsServer, 100);
  _ProvisionConfiguration.endpointName = strndup(netCfg->endpointName, 24);

  //Server configuration
  provision_decodeBytes(_DelayedServerConfigData, _DelayedServerConfigDataLen);
  Provision_DeviceServerConfig* cfg = (Provision_DeviceServerConfig*)_DelayedServerConfigData;
  _ProvisionConfiguration.securityMode = cfg->securityMode;
  _ProvisionConfiguration.pskKeySize = cfg->pskKeySize;
  _ProvisionConfiguration.pskKey = malloc(cfg->pskKeySize);
  memcpy(_ProvisionConfiguration.pskKey, cfg->psk, cfg->pskKeySize);
  _ProvisionConfiguration.bootstrapUri = strndup(cfg->bootstrapUri, 200);

#ifdef PROVISION_DEBUG
  printf("Provision: defaultRouteUri=%s\n", _ProvisionConfiguration.defaultRouteUri);
  printf("Provision: dnsServer=%s\n", _ProvisionConfiguration.dnsServer);
  printf("Provision: endpointName=%s\n", _ProvisionConfiguration.endpointName);
  printf("Provision: bootstrapUri=%s\n", _ProvisionConfiguration.bootstrapUri);
#endif
}

static size_t strnlen(const char * s, size_t len) {
  size_t i = 0;
  for( ; i < len && s[i] != '\0'; ++i);
  return i;
}

static void cleanupAndBootMasterProcess() {
#ifdef PROVISION_DEBUG
  printf("Provision: Continue boot process.\n");
#endif

  provision_communicationCleanup();
  provision_cryptoCleanup();
  FREE_AND_NULL(_DelayedServerConfigData);
  FREE_AND_NULL(_DelayedNetworkConfigData);

  if (_ConfigCallback != NULL) {
      _ConfigCallback(&_ProvisionConfiguration);
  }
  GPIO_CLR(LED1_PORT, LED1_PIN);
  GPIO_CLR(LED2_PORT, LED2_PIN);

#ifdef PROVISION_DEBUG
  printf("Provision: Done.\n");
#endif

  process_start(_ProcessToRun, NULL);
}

void provision_boot(struct process *processToRun, ProvisionConfigCallback configCallback)
{
  _ProcessToRun = processToRun;
  _ConfigCallback = configCallback;

  //configure leds
  GPIO_CONFIGURE_AS_DIGITAL(LED1_PORT, LED1_PIN);
  GPIO_CONFIGURE_AS_OUTPUT(LED1_PORT, LED1_PIN);
  GPIO_CONFIGURE_AS_DIGITAL(LED2_PORT, LED2_PIN);
  GPIO_CONFIGURE_AS_OUTPUT(LED2_PORT, LED2_PIN);

  if (provision_hasConfigData()) {
    _BootMode = Provision_BootMode_MODE_HAS_CONFIG;
#ifdef PROVISION_DEBUG
    printf("Provision: run in CONFIGURED MODE\n");
#endif
    process_start(&provisionProcessConfigured, NULL);

  } else {
    _BootMode = Provision_BootMode_MODE_NO_CONFIG;
#ifdef PROVISION_DEBUG
    printf("Provision: run in NO CONFIG MODE\n");
#endif
    process_start(&provisionProcessNoConfig, NULL);
  }
}
