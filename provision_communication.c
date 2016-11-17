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

#include "contiki.h"
#include "contiki-net.h"
#include "provision_communication.h"
#include "helpers.h"

#define IN_OUT_BUFSIZE 100

uip_ds6_defrt_t* _ProvisionRouteRef = NULL;
static struct tcp_socket _Socket;
static bool _SocketNeedCleanup = false;
static uint8_t _InputBuf[IN_OUT_BUFSIZE];
static uint8_t _OutputBuf[IN_OUT_BUFSIZE];
static Provision_NetworkCommandHandlerBind* _CommandHandlers = NULL;
static Provision_CommunicationListener _CommunicationListener = NULL;

//buffer for collecting data for command
static Provision_NetworkCommand _ProcessedCommand = {.dataPtr = NULL, .dataSize = 0};
static uint8_t cmdDataExpectedSize = 0;   //0 - no data expected,
                                          //255 - data size need to be read on next inputHandler call,
                                          //other values are real data size

static Provision_NetworkCommandHandlerBind findHandlerFor(Provision_NetworkCommandType cmd) {
  Provision_NetworkCommandHandlerBind* binder = _CommandHandlers;
  while( ( binder->command != Provision_NetworkCommandType_NONE) && ( binder->command != cmd)) {
    binder ++;
  }

  if (binder->command == Provision_NetworkCommandType_NONE) {
    //no handler for this command found
#ifdef PROVISION_DEBUG
    printf("Provision: No handler for command: %d\n", cmd);
#endif
  }

  return *binder;
}

static int getNextBytesFromStream(uint8_t* dest, uint8_t count, uint8_t** buffer, int* bufferSize) {
  count = count > *bufferSize ? *bufferSize : count;
  if (count == 0) {
    return 0;
  }

  memcpy(dest, *buffer, count);
  *buffer += count;
  *bufferSize -= count;

  return count;
}

//return 0 -> ok we have all, !0 still need more data
static int collectCommandData(uint8_t** buffer, int* bufferSize) {
  int count = getNextBytesFromStream(&_ProcessedCommand.dataPtr[_ProcessedCommand.dataSize],
                                     cmdDataExpectedSize - _ProcessedCommand.dataSize,
                                     buffer, bufferSize);
  _ProcessedCommand.dataSize += count;
#ifdef PROVISION_DEBUG
  printf("Provision: collecting data, this round:%d, total: %d, still to go:%d\n", count, _ProcessedCommand.dataSize,
    cmdDataExpectedSize - _ProcessedCommand.dataSize);
#endif
  return _ProcessedCommand.dataSize == cmdDataExpectedSize;
}

static int inputHandler(struct tcp_socket* s, void* ptr, const uint8_t* inputPtr, int inputDataLen) {
#ifdef PROVISION_DEBUG
  printf("Provision: Bytes read %d, ptr:%p\n", inputDataLen, inputPtr);
  if (inputPtr != NULL) {
    PRINT_BYTES(inputPtr, inputDataLen);
  }
#endif

  uint8_t* buffer = (uint8_t*)inputPtr;
  while(true) {
    if (cmdDataExpectedSize == 255) {
      //we got command which expected data, let's try obtain data size
      if (getNextBytesFromStream(&cmdDataExpectedSize, 1, &buffer, &inputDataLen) == 0) {
        //no info about datasize, we will wait to get this info from network
        cmdDataExpectedSize = 255;
        return 0;
      }
#ifdef PROVISION_DEBUG
      printf("Provision: CMD, expected data size: %d\n", cmdDataExpectedSize);
#endif
      //ok we know how many data to collect, alloc buffer for it
      _ProcessedCommand.dataPtr = malloc(cmdDataExpectedSize);
      _ProcessedCommand.dataSize = 0;  //this will be incremeted in collectCommandData()
    }

    //we want to collect some expected data
    if (cmdDataExpectedSize > 0) {
      //we got expected data size, now read expected amount of data
      if (collectCommandData(&buffer, &inputDataLen) != 0) {
        //ok, we got what we wanted, create Provision_NetworkCommand and execute it
        Provision_NetworkCommandHandlerBind binder = findHandlerFor(_ProcessedCommand.command);
        if (binder.handler != NULL) {
          binder.handler(_ProcessedCommand);
        }
        FREE_AND_NULL(_ProcessedCommand.dataPtr);
        _ProcessedCommand.dataSize = 0;
        cmdDataExpectedSize = 0;

      } else {
        //we lack some data, will be setup on next call.
        return 0;
      }
    }

    //we expecting a new command
    uint8_t tmp;
    if (getNextBytesFromStream(&tmp, 1, &buffer, &inputDataLen) == 0) {
      _ProcessedCommand.command = Provision_NetworkCommandType_NONE;
      cmdDataExpectedSize = 0;
      return 0; //no more data to process
    }
    _ProcessedCommand.command = tmp;
    //find proper handler for received command
    Provision_NetworkCommandHandlerBind binder = findHandlerFor(_ProcessedCommand.command);
    if (binder.command == Provision_NetworkCommandType_NONE) {
      continue;
    }
    if (binder.dataIsExpected == false) {
      //no data needed just execute
      binder.handler(_ProcessedCommand);
      _ProcessedCommand.command = Provision_NetworkCommandType_NONE;
      cmdDataExpectedSize = 0;

    } else {
      //set marker that we need data size + data to collect
      cmdDataExpectedSize = 255;
    }
  }

  return 0;
}

static void eventHandler(struct tcp_socket *s, void *ptr, tcp_socket_event_t ev) {
#ifdef PROVISION_DEBUG
  char* eventNames[] = {
    "TCP_SOCKET_CONNECTED", "TCP_SOCKET_CLOSED", "TCP_SOCKET_TIMEDOUT",
    "TCP_SOCKET_ABORTED", "TCP_SOCKET_DATA_SENT"
  };
  printf("Provision: tcp event %s\n", eventNames[ev]);
#endif

  switch(ev) {
    case TCP_SOCKET_CLOSED:
    case TCP_SOCKET_TIMEDOUT:
    case TCP_SOCKET_ABORTED:
      _CommunicationListener(Provision_CommunicationState_DISCONNECTED);
      provision_closeConnection();
      break;

    case TCP_SOCKET_CONNECTED:
      _CommunicationListener(Provision_CommunicationState_CONNECTED);
      break;

    case TCP_SOCKET_DATA_SENT:
      //send data is finished
      break;
  }
}

void provision_setCommandHandlers(Provision_NetworkCommandHandlerBind* handlers, Provision_CommunicationListener stateListener) {
  _CommandHandlers = handlers;
  _CommunicationListener = stateListener;
}

uint8_t provision_openIPConnection(char* ip, int port, uip_ipaddr_t* addr) {
  static uip_ipaddr_t tmpAddr;
  if (addr != NULL) {
    memcpy(&tmpAddr, addr, sizeof(uip_ipaddr_t));
  }
  if (ip != NULL) {
    if (uiplib_ipaddrconv(ip, &tmpAddr) == 0) {
      return -1;
    }
  }

#ifdef PROVISION_DEBUG
    printf("Provision: Starting connection:\n");
    PRINT6ADDR(&tmpAddr);
    printf("\n");
#endif
  _SocketNeedCleanup = true;
  int result = tcp_socket_register(&_Socket, NULL,
                _InputBuf, sizeof(_InputBuf),
                _OutputBuf, sizeof(_OutputBuf),
                inputHandler, eventHandler);
  result += tcp_socket_connect(&_Socket, &tmpAddr, port);

  return result == 2 ? 0 : -1;
}

void provision_closeConnection() {
  if (_SocketNeedCleanup) {
#ifdef PROVISION_DEBUG
  printf("Provision: Closing connection\n");
#endif
    tcp_socket_close(&_Socket);
    tcp_socket_unregister(&_Socket);

    memset(&_Socket, 0, sizeof(_Socket));
    _SocketNeedCleanup = false;
  }
}

uint8_t provision_sendData(uint8_t* data, uint8_t size) {
#ifdef PROVISION_DEBUG
  printf("Provision: Sending data, size: %d\n", size);
#endif
  int result = tcp_socket_send(&_Socket, data, size);
  return result == -1 ? -1 : 0;
}

uint8_t provision_setDefaultRoute(uip_ipaddr_t* iaddr) {
#ifdef PROVISION_DEBUG
  printf("Provision: Default route set to:");
  PRINT6ADDR(iaddr);
  printf("\n");
#endif

  _ProvisionRouteRef = uip_ds6_defrt_add(iaddr, 0);
  PRINT6ADDR(iaddr);
  return _ProvisionRouteRef == NULL ? -1 : 0;
}

void provision_communicationCleanup() {
#ifdef PROVISION_DEBUG
  printf("Provision: Cleanup\n");
#endif
  provision_closeConnection();
  if (_ProvisionRouteRef != NULL) {
    uip_ds6_defrt_rm(_ProvisionRouteRef);
    _ProvisionRouteRef = NULL;
  }
  FREE_AND_NULL(_ProcessedCommand.dataPtr);
}
