/**
 * @file
 * Provisioning library crypto module.
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
#ifndef __PROVISION_CRYPTO__
#define __PROVISION_CRYPTO__

#include <stdint.h>
#include <stdbool.h>
#include "sys/process.h"

/**
 * @brief Allocate result buffer and encode given source bufffer. Rijndael AES128 in CTR mode is used.
 * @param[in] src buffer to encode, data inside will be changed!
 * @param[in] len size of source buffer
 * @param[in] key Key used in encryption, it should be 16 bytes long
 * @param[out] outputSize size of returned encrypted buffer
 * @return pointer to buffer with encrypted data.
 */
uint8_t* provision_encodeBytes(uint8_t* src, uint8_t len, uint8_t* key, uint8_t* outputSize);

/**
 * @brief Decode data encoded by encodeBytes function. Result of decryption is placed into this same source buffer.
 * @param[in,out] encrypted data to decode, decoded data after call
 * @param[in] len size of encrypted data
 */
void provision_decodeBytes(uint8_t* data, uint8_t len);

void provision_cryptoCleanup();

void provision_preparePublicKey();
uint8_t* provision_getPublicKey(uint8_t* keyLen);

void provision_setServerPublicKey(uint8_t* key, uint8_t keyLength);

void provision_finalizeKeyExchange();

bool provision_readyForDecrypt();

#endif
