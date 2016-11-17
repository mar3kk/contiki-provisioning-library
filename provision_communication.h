/**
 * @file
 * Provisioning library communication module.
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
#include <stdbool.h>

#ifndef __PROVISION_COMMUNICATION_H__
#define __PROVISION_COMMUNICATION_H__

typedef enum {
  Provision_NetworkCommandType_NONE = 0,
  Provision_NetworkCommandType_ENABLE_HIGHLIGHT = 1,
  Provision_NetworkCommandType_DISABLE_HIGHLIGHT,
  Provision_NetworkCommandType_KEEP_ALIVE,
  Provision_NetworkCommandType_PUBLIC_KEY,
  Provision_NetworkCommandType_SERVER_CONFIG,
  Provision_NetworkCommandType_NETWORK_CONFIG,

  Provision_NetworkCommandType_LAST
} Provision_NetworkCommandType;

typedef struct {
  Provision_NetworkCommandType command;
  uint8_t dataSize; //might be 0, then dataPtr == NULL
  uint8_t* dataPtr; //reciever becomming owner and must free this!
} Provision_NetworkCommand;

typedef enum {
  Provision_CommunicationState_CONNECTED,
  Provision_CommunicationState_DISCONNECTED
} Provision_CommunicationState;

typedef void (*Provision_NetworkCommandHandler)(Provision_NetworkCommand cmd);
typedef void (*Provision_CommunicationListener)(Provision_CommunicationState newState);

typedef struct {
  Provision_NetworkCommandType command;
  bool dataIsExpected;
  Provision_NetworkCommandHandler handler;
} Provision_NetworkCommandHandlerBind;

//struct for Provision_NetworkCommandType_SERVER_CONFIG
typedef struct __attribute__((__packed__)) {
    uint8_t securityMode;
    uint8_t pskKeySize;
    uint8_t psk[32];
    uint8_t bootstrapUri[200];
} Provision_DeviceServerConfig;

//struct for Provision_NetworkCommandType_NETWORK_CONFIG
typedef struct __attribute__((__packed__)) {
  char defaultRouteUri[100];  //can be IPv6 or URL
  char dnsServer[100];
  char endpointName[24];
} Provision_NewtworkConfig;

void provision_setCommandHandlers(Provision_NetworkCommandHandlerBind* handlers, Provision_CommunicationListener stateListener);
/**
 * @return 0 on success, -1 if error
 */
uint8_t provision_setDefaultRoute(uip_ipaddr_t* iaddr);

/**
 * @return 0 on success, -1 if error
 */
uint8_t provision_openIPConnection(char* ip, int port, uip_ipaddr_t* iaddr);
void provision_closeConnection();
uint8_t provision_sendData(uint8_t* data, uint8_t size);
void provision_communicationCleanup();

#endif
