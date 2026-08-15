#ifndef KYTY_COMMON_HASH_H_
#define KYTY_COMMON_HASH_H_

#include "common/common.h"
#include <xxhash.h>

namespace Common {

inline uint32_t hash(const void* key, uint32_t key_len) {
	if (key == nullptr || key_len == 0) {
		return 0;
	}
	return static_cast<uint32_t>(XXH3_64bits(key, static_cast<size_t>(key_len)));
}

inline uint64_t hash64_data(const void* key, size_t key_len) {
	if (key == nullptr || key_len == 0) {
		return 0;
	}
	return XXH3_64bits(key, key_len);
}

inline uint32_t hash8(uint8_t key) {
	return static_cast<uint32_t>(XXH3_64bits(&key, sizeof(key)));
}

inline uint32_t hash16(uint16_t key) {
	return static_cast<uint32_t>(XXH3_64bits(&key, sizeof(key)));
}

inline uint32_t hash32(uint32_t key) {
	return static_cast<uint32_t>(XXH3_64bits(&key, sizeof(key)));
}

inline uint32_t hash64(uint64_t key) {
	return static_cast<uint32_t>(XXH3_64bits(&key, sizeof(key)));
}

} // namespace Common

#endif /* KYTY_COMMON_HASH_H_ */
