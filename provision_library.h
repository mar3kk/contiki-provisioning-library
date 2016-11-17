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

#ifndef PROVISION_WAIT_TIME_TO_BOOT
#define PROVISION_WAIT_TIME_TO_BOOT 8000
#endif

#ifndef PROVISION_DAEMON
//Here is name for mDNS resolve, this library will try to establish connetion with it.
#define PROVISION_DAEMON "OpenWrt.local"
#endif

#ifndef PROVISION_DAEMON_PORT
#define PROVISION_DAEMON_PORT 49300
#endif

#include "provision_config.h"

#ifndef __PROVISION_LIBRARY__
#define __PROVISION_LIBRARY__

/**
 * @brief Type of callback which will be called when configuration is loaded/sent by user.
 * @param config pointer to configuration structure
 */
typedef void (*ProvisionConfigCallback)(Provision_Configuration* config);

/**
 * @brief Boots provision process. This method should always be called as a entry point to application. It verifies if
 * proper configuration is present. If no then provisioning mode is enabled, user can configure required fields and
 * request boot again. If config is already present user will have some time to press button which reset config, or
 * just wait. After few seconds processToRun will be executed. But before that call to configCallback is done to
 * deliver configuration.
 * @param processToRun pointer to process structure, this process will be executed after valid configuration is present
 * @param configCallback pointer to callback method, this method will be called with valid configuration.
 */
void provision_boot(struct process *processToRun, ProvisionConfigCallback configCallback);

#endif
