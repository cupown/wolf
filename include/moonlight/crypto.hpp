#pragma once

#include <openssl/aes.h>
#include <openssl/x509.h>
#include <string>

namespace crypto {

std::string sha256(const std::string &str);
std::string pem(const X509 &x509);
std::string str_to_hex(const std::string &input);
std::string hex_to_str(const std::string &hex, bool reverse);

std::string random(int length);

/**
 * Encrypt the given msg using AES cbc at 128 bit
 *
 * @param msg: the message to be encrypted
 * @param enc_key: the key used for encryption
 * @param iv: optional, if not provided a random one will be generated
 * @param padding: optional, enables or disables padding
 * @return: the encrypted string
 */
std::string aes_encrypt_cbc(const std::string &msg,
                            const std::string &enc_key,
                            const std::string &iv = random(AES_BLOCK_SIZE),
                            bool padding = true);

/**
 * Decrypt the given msg using AES cbc at 128 bit
 *
 * @param msg: the message to be encrypted
 * @param enc_key: the key used for encryption
 * @param iv: optional, if not provided a random one will be generated
 * @param padding: optional, enables or disables padding
 * @return: the decrypted string
 */
std::string aes_decrypt_cbc(const std::string &msg,
                            const std::string &enc_key,
                            const std::string &iv = random(AES_BLOCK_SIZE),
                            bool padding = true);

} // namespace crypto