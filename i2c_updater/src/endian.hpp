/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ENDIAN_HPP_
#define _ENDIAN_HPP_

#include <cstdint>
// #include <bit> // we don't have GCC with C++20 on raspi :(

namespace nEndian {

template<typename t_tVal>
constexpr t_tVal swapBytes(t_tVal Val) {
	t_tVal Out = 0;
	for(size_t i = 0; i < sizeof(Val); ++i) {
		Out = (Out << 8) | (Val & 0xFF);
		Val >>= 8;
	}
	return Out;
}

template<typename t_tVal>
constexpr t_tVal littleToNative(const t_tVal &Val) {
	// if(std::endian::native == std::endian::big) {
	// 	return swapBytes(Val);
	// }
	// else {
		return Val;
	// }
}

template<typename t_tVal>
constexpr auto nativeToLittle = littleToNative<t_tVal>;

template<typename t_tVal>
constexpr t_tVal bigToNative(const t_tVal &Val) {
	// if(std::endian::native == std::endian::big) {
	// 	return Val;
	// }
	// else {
		return swapBytes(Val);
	// }
}

template<typename t_tVal>
constexpr auto nativeToBig = bigToNative<t_tVal>;


} // namespace nEndian

#endif // _ENDIAN_HPP_
