/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _I2C_HPP_
#define _I2C_HPP_

#include <string>
#include <vector>

class tI2c {
public:
	tI2c(const std::string &Port);

	bool write(uint8_t ubAddr, const std::vector<uint8_t> &vData);

	bool read(uint8_t ubAddr, uint8_t *pDest, uint32_t ulReadSize);

	template <typename t_tContainer>
	bool read(uint8_t ubAddr, t_tContainer &Cont) {
		return read(ubAddr, Cont.data(), Cont.size());
	}

private:
	int m_I2cHandle;
};

#endif // _I2C_HPP_
