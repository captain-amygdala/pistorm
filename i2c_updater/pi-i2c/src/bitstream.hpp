/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _BITSTREAM_HPP_
#define _BITSTREAM_HPP_

#include <string>
#include <vector>
#include <array>
#include <fstream>

struct tBitstream {
	tBitstream(const std::string &FilePath);

	std::array<uint8_t, 4> m_DeviceId;
	uint32_t m_ulCtlReg0;
	std::array<uint8_t, 4> m_UserCode;
	std::vector<std::string> m_vComments;
	std::vector<std::vector<uint8_t>> m_vProgramData;
};

#endif // _BITSTREAM_HPP_
