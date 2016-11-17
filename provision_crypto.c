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
#include <stdlib.h>
#include <string.h>
#include "provision_crypto.h"
#include "rijndael.h"
#include "diffie_hellman_keys_exchanger.h"
#include "helpers.h"

#define CRYPTO_G_MODULE 3423789
#define EXC_KEY_LENGTH 16

static uint8_t* serverPublicKey = NULL;
static uint8_t* publicKey = NULL;
static uint8_t* sharedKey = NULL;

static uint8_t primeNumber[EXC_KEY_LENGTH] = {
  //dec: 316649221753004337541162510835822305309, 128 bit
  0xEE, 0x38, 0x6B, 0xFB,
  0x5A, 0x89, 0x9F, 0xA5,
  0xAE, 0x9F, 0x24, 0x11,
  0x7C, 0x4B, 0x20, 0x1D};

static DiffieHellmanKeysExchanger* keyExchanger = NULL;

static bool provision_random(unsigned char* num, int count) {
#ifdef USE_CC2520
  cc2520_get_random(num, count);
#endif
#ifdef USE_CA8210
  ca8210_get_random(num, count);
#endif
  return true;
}

uint8_t* provision_encodeBytes(uint8_t* src, uint8_t len, uint8_t* key, uint8_t* outputSize) {
  uint8_t IV[16];
  int t;
  for(t = 0; t < 15; t++) { //spare the last byte
    IV[t] = key[15 - t];
  }
#ifdef PROVISION_DEBUG
  printf("Provision: IV is ");
  PRINT_BYTES(IV, 16);
#endif
  int paddedSize = (len / 16) * 16;
  if (paddedSize < len) {
    paddedSize += 16;
  }
  *outputSize = paddedSize;

  uint8_t* result = malloc(paddedSize);

  rijndael_ctx ctx;
  rijndael_set_key(&ctx, key, 128);

  int y;
  for(t = 0; t < paddedSize; t += 16) {
    IV[15] = t / 16;
    for(y = 0; y < 16; y++) {
      src[t+y] ^= IV[y];
    }
    rijndael_encrypt(&ctx, src + t, result + t);
  }

  return result;
}

void provision_decodeBytes(uint8_t* data, uint8_t len) {
  uint8_t IV[16];
  int t;
  for(t = 0; t < 15; t++) { //spare the last byte
    IV[t] = sharedKey[15 - t];
  }
#ifdef PROVISION_DEBUG
  printf("Provision: IV is ");
  PRINT_BYTES(IV, 16);
#endif

  rijndael_ctx ctx;
  rijndael_set_key(&ctx, sharedKey, 128);

  uint8_t tmp[16];
  int y;
  for(t = 0; t < len; t += 16) {
    rijndael_decrypt(&ctx, data + t, tmp);
    IV[15] = t / 16;
    for(y = 0; y < 16; y++) {
      tmp[y] ^= IV[y];
    }
    memcpy(data + t, tmp, 16);
  }
}

void provision_setServerPublicKey(uint8_t* key, uint8_t keyLength) {
  if (serverPublicKey != NULL) {
    free(serverPublicKey);
  }
  serverPublicKey = malloc(keyLength);
  memcpy(serverPublicKey, key, keyLength);
#ifdef PROVISION_DEBUG
  if (keyLength != EXC_KEY_LENGTH) {
    printf("Provision: public key length is invalid, expected: %d but got: %d",EXC_KEY_LENGTH, keyLength);
  }
#endif
}

void provision_preparePublicKey() {
  bi_generateConst();
  if (keyExchanger == NULL) {
    keyExchanger = dh_newKeyExchanger(primeNumber, EXC_KEY_LENGTH, CRYPTO_G_MODULE, provision_random);
#ifdef PROVISION_DEBUG
    printf("Provision: generating public key...\n" );
#endif
  }
  publicKey = dh_generateExchangeData(keyExchanger);
}

void provision_finalizeKeyExchange() {
  if ((sharedKey == NULL) && (publicKey != NULL) && (serverPublicKey != NULL) ) {
#ifdef PROVISION_DEBUG
    printf("Provision: generating shared key...\n" );
#endif
    sharedKey = dh_completeExchangeData(keyExchanger, serverPublicKey, EXC_KEY_LENGTH);
#ifdef PROVISION_DEBUG
    printf("Provision: shared key is\n" );
    PRINT_BYTES(sharedKey, EXC_KEY_LENGTH);
#endif
  }
#ifdef PROVISION_DEBUG
  else {
    printf("Provision: provision_finalizeKeyExchange, can't start sharedKey:%d, publicKey:%d, serverPublicKey:%d\n",
      (sharedKey == NULL), (publicKey != NULL), (serverPublicKey != NULL));
  }
#endif
}

uint8_t* provision_getPublicKey(uint8_t* keyLen) {
  if (keyLen != NULL) {
    *keyLen = EXC_KEY_LENGTH;
  }
  return publicKey;
}

bool provision_readyForDecrypt() {
  return sharedKey != NULL;
}

void provision_cryptoCleanup() {
  bi_releaseConst();
  FREE_AND_NULL(serverPublicKey);
  FREE_AND_NULL(publicKey);
  FREE_AND_NULL(sharedKey);
  dh_release(&keyExchanger);
}
