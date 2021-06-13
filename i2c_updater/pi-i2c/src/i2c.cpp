/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "i2c.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdexcept>

tI2c::tI2c(const std::string &Port)
{
	// Open the I2C bus file handle
	m_I2cHandle = open(Port.c_str(), O_RDWR);
	if(m_I2cHandle < 0) {
		// TODO: check errno to see what went wrong
		throw std::runtime_error("Can't open the i2c bus\n");
	}
}

bool tI2c::write(uint8_t ubAddr, const std::vector<uint8_t> &vData) {
	if(ioctl(m_I2cHandle, I2C_SLAVE, ubAddr) < 0) {
		// NOTE: check errno to see what went wrong
		return false;
	}

	auto BytesWritten = ::write(m_I2cHandle, vData.data(), vData.size());
	if(BytesWritten != ssize_t(vData.size())) {
		return false;
	}

	return true;
}

bool tI2c::read(uint8_t ubAddr, uint8_t *pDest, uint32_t ulReadSize) {
	if(ioctl(m_I2cHandle, I2C_SLAVE, ubAddr) < 0) {
		// NOTE: check errno to see what went wrong
		return false;
	}

	auto BytesRead = ::read(m_I2cHandle, pDest, ulReadSize);
	if(BytesRead != ssize_t(ulReadSize)) {
		return false;
	}

	return true;
}
