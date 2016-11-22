/**
 * @file
 * Provisioning library config storage module.
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "provision_config.h"
#include "cfs/cfs-coffee.h"
#include "core/dev/watchdog.h"
#include "helpers.h"

#define CONFIG_FILE "PROV_CONFIG"

Provision_Configuration _ProvisionConfiguration = {
  .securityMode = 0,
  .pskKeySize = 0,
  .pskKey = NULL,
  .bootstrapUri = NULL,
  .defaultRouteUri = NULL,
  .endpointName = NULL,
  .dnsServer = NULL
};
static bool isLoaded;

static inline void provision_fatalError() {
  watchdog_reboot();
}

static void readString(int fileHandle, char** dest)
{
  if (*dest != NULL) {
      free(*dest);
      *dest = NULL;
  }
  uint16_t len;
  int result = cfs_read(fileHandle, &len, sizeof(len));
  if (result < 0) {
#ifdef PROVISION_DEBUG
      printf("Provision: Error %d - while loaging config\n", result);
#endif
      provision_fatalError();
  }
  if (len == 0) {
      *dest = NULL;
      return;
  }
  *dest = malloc(len + 1);
  result = cfs_read(fileHandle, *dest, len);
  if (result < 0) {
#ifdef PROVISION_DEBUG
      printf("Provision: Error %d - while loaging config\n", result);
#endif
      provision_fatalError();
  }
  *(dest + len) = 0;
}

static bool writeString(int fileHandle, char* src)
{
  uint16_t len = strlen(src);
  int result = cfs_write(fileHandle, &len, sizeof(len));
  if (result < 0) {
      return false;
  }
  if (len > 0) {
      result = cfs_write(fileHandle, src, len);
      if (result < 0) {
          return false;
      }
  }
  return true;
}

bool provision_hasConfigData() {
  int fileHandle = cfs_open(CONFIG_FILE, CFS_READ);
  cfs_close(fileHandle);
#ifdef PROVISION_DEBUG
    printf("Provision: has config:%d.\n", fileHandle >= 0);
#endif
  return fileHandle >= 0;
}

void provision_loadConfig() {
#ifdef PROVISION_DEBUG
  printf("Provision: Loading config\n");
#endif
  int fileHandle = cfs_open(CONFIG_FILE, CFS_READ);
  if (fileHandle < 0) {
      printf("Provision: Error - while opening config\n");
      provision_fatalError();
  }

  bool success = cfs_read(fileHandle, &_ProvisionConfiguration.securityMode,
    sizeof(_ProvisionConfiguration.securityMode)) >= 0;

  success &= cfs_read(fileHandle, &_ProvisionConfiguration.pskKeySize,
    sizeof(_ProvisionConfiguration.pskKeySize)) >= 0;

  if (success == false) {
#ifdef PROVISION_DEBUG
      printf("Provision: Error - while loaging config\n");
#endif
      provision_fatalError();
  }

  readString(fileHandle, (char**)&_ProvisionConfiguration.pskKey);
  readString(fileHandle, &_ProvisionConfiguration.bootstrapUri);
  readString(fileHandle, &_ProvisionConfiguration.defaultRouteUri);
  readString(fileHandle, &_ProvisionConfiguration.endpointName);
  readString(fileHandle, &_ProvisionConfiguration.dnsServer);

  cfs_close(fileHandle);
}

static bool innerSaveConfig(int* outFileHandle) {
  int fileHandle = cfs_open(CONFIG_FILE, CFS_WRITE);
  *outFileHandle = fileHandle;
  if(fileHandle < 0) {
      return false;
  }

  bool saveResult = true;
  saveResult &= cfs_write(fileHandle, &_ProvisionConfiguration.securityMode,
    sizeof(_ProvisionConfiguration.securityMode)) >= 0;
  saveResult &= cfs_write(fileHandle, &_ProvisionConfiguration.pskKeySize,
    sizeof(_ProvisionConfiguration.pskKeySize)) >= 0;

  saveResult &= writeString(fileHandle, _ProvisionConfiguration.pskKey);
  saveResult &= writeString(fileHandle, _ProvisionConfiguration.bootstrapUri);
  saveResult &= writeString(fileHandle, _ProvisionConfiguration.defaultRouteUri);
  saveResult &= writeString(fileHandle, _ProvisionConfiguration.endpointName);
  saveResult &= writeString(fileHandle, _ProvisionConfiguration.dnsServer);

  cfs_close(fileHandle);
  *outFileHandle = 0;
  return saveResult;
}

void provision_saveConfig() {
#ifdef PROVISION_DEBUG
  printf("Provision: Saving config\n");
#endif
  int fileHandle = 0;
  if (innerSaveConfig(&fileHandle) == false) {
    if (fileHandle != 0) {
        cfs_close(fileHandle);
    }
#ifdef PROVISION_DEBUG
    printf("Provision: Error while writting config\n");
#endif
    provision_clearConfig();
    provision_fatalError();
  }
}

void provision_clearConfig() {
#ifdef PROVISION_DEBUG
  printf("Provision: Removing config\n");
#endif
  if (isLoaded) {
    FREE_AND_NULL(_ProvisionConfiguration.pskKey);
    FREE_AND_NULL(_ProvisionConfiguration.bootstrapUri);
    FREE_AND_NULL(_ProvisionConfiguration.defaultRouteUri);
    FREE_AND_NULL(_ProvisionConfiguration.endpointName);
    FREE_AND_NULL(_ProvisionConfiguration.dnsServer);
    _ProvisionConfiguration.pskKeySize = 0;
    _ProvisionConfiguration.securityMode = 0;
    isLoaded = false;
  }
  cfs_remove(CONFIG_FILE);
}

void provision_verifyConfig() {
#ifdef PROVISION_DEBUG
  printf("Provision: Verify received config.\n");
#endif

  if (strlen(_ProvisionConfiguration.defaultRouteUri) == 0) {
#ifdef PROVISION_DEBUG
    printf("Provision: Fatal - no Default Route URI is given\n");
#endif
    provision_fatalError();
  }

  if (strlen(_ProvisionConfiguration.bootstrapUri) == 0) {
#ifdef PROVISION_DEBUG
    printf("Provision: Fatal - no bootstrapURI is given\n");
#endif
    provision_fatalError();
  }

  if (strlen(_ProvisionConfiguration.endpointName) == 0) {
#ifdef PROVISION_DEBUG
    printf("Provision: Fatal - no Endpoint Name is given\n");
#endif
    provision_fatalError();
  }
}
