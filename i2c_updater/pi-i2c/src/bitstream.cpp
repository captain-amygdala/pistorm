/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bitstream.hpp"
#include <sstream>
#include <iomanip>
#include "file.hpp"

// Based on https://prjtrellis.readthedocs.io/en/latest/architecture/bitstream_format.html ,
// Lattice Diamond programmer operation's I2C dump and "ECP5 and ECP5-5G sysCONFIG Usage Guide"

#define SECTION_ASCII_COMMENTS_BEGIN 0xFF00
#define SECTION_PREAMBLE_WORD_1 0xFFFF
#define SECTION_PREAMBLE_WORD_2 0xBDB3

struct tBitstreamCmd {
	enum class tId: uint8_t {
		LSC_PROG_CNTRL0      = 0x22,
		LSC_RESET_CRC        = 0x3B,
		LSC_INIT_ADDRESS     = 0x46,
		ISC_PROGRAM_DONE     = 0x5E,
		LSC_PROG_INCR_RTI    = 0x82,
		LSC_PROG_SED_CRC     = 0xA2,
		LSC_EBR_WRITE        = 0xB2,
		ISC_PROGRAM_USERCODE = 0xC2,
		ISC_PROGRAM_SECURITY = 0xCE,
		LSC_VERIFY_ID        = 0xE2,
		LSC_EBR_ADDRESS      = 0xF6,
		ISC_NOOP             = 0xFF,
	};

	tBitstreamCmd::tId m_eId;
	std::array<uint8_t, 3> m_Operands;
	std::vector<uint8_t> m_vData;

	tBitstreamCmd(std::ifstream &FileIn);
};

tBitstream::tBitstream(const std::string &FilePath)
{
	std::ifstream FileIn;
	FileIn.open(FilePath.c_str(), std::ifstream::in | std::ifstream::binary);
	if(!FileIn.good()) {
		throw std::runtime_error("Couldn't open file");
	}

	uint16_t uwSectionId;
	nFile::readBigEndian(FileIn, uwSectionId);
	if(uwSectionId == SECTION_ASCII_COMMENTS_BEGIN) {
		// Read comments
		std::string Comment;
		char c;
		while(!FileIn.eof()) {
			nFile::readBigEndian(FileIn, c);
			if(c == '\xFF') {
				break;
			}
			if(c == '\0') {
				m_vComments.push_back(Comment);
				Comment.clear();
			}
			else {
				Comment += c;
			}
		}

		// Continue reading until we hit preamble
		nFile::readBigEndian(FileIn, uwSectionId);
	}

	if(uwSectionId != SECTION_PREAMBLE_WORD_1) {
		// I want std::format so bad :(
		std::stringstream ss;
		ss << "Unexpected bitstream section: " << std::setfill('0') <<
			std::setw(4) << std::ios::hex << uwSectionId;
		throw std::runtime_error(ss.str());
	}

	// Read final part of preamble
	uint16_t uwPreambleEnd;
	nFile::readBigEndian(FileIn, uwPreambleEnd);
	if(uwPreambleEnd != SECTION_PREAMBLE_WORD_2) {
		// I want std::format so bad :(
		std::stringstream ss;
		ss << "Unexpected preamble ending" << std::setfill('0') <<
			std::setw(4) << std::ios::hex << uwPreambleEnd;
		throw std::runtime_error(ss.str());
	}

	while(!FileIn.eof()) {
		// Read bitstream commands
		tBitstreamCmd Cmd(FileIn);
		if(FileIn.eof()) {
			// Last read haven't been successfull - no more data
			break;
		}

		// Read extra data, if any
		switch(Cmd.m_eId) {
			case tBitstreamCmd::tId::ISC_NOOP:
			case tBitstreamCmd::tId::LSC_RESET_CRC:
			case tBitstreamCmd::tId::LSC_INIT_ADDRESS:
				// No additional data
				break;
			case tBitstreamCmd::tId::LSC_VERIFY_ID:
				nFile::readData(FileIn, m_DeviceId.data(), m_DeviceId.size());
				break;
			case tBitstreamCmd::tId::LSC_PROG_CNTRL0:
				nFile::readBigEndian(FileIn, m_ulCtlReg0);
				break;
			case tBitstreamCmd::tId::LSC_PROG_INCR_RTI: {
				// Store program data
				// bool isCrcVerify = Cmd.m_Operands[0] & 0x80;
				// bool isCrcAtEnd = !(Cmd.m_Operands[0] & 0x40);
				// bool isDummyBits = !(Cmd.m_Operands[0] & 0x20);
				// bool isDummyBytes = !(Cmd.m_Operands[0] & 0x10);
				auto DummyBytesCount = Cmd.m_Operands[0] & 0xF;
				uint16_t uwRowCount = (Cmd.m_Operands[1] << 8) | Cmd.m_Operands[2];

				for(auto RowIdx = 0; RowIdx < uwRowCount; ++RowIdx) {
					// TODO: where to get the size of each data row? From comment?
					std::vector<uint8_t> vRow;
					vRow.resize(26);
					uint16_t uwCrc;
					nFile::readData(FileIn, vRow.data(), vRow.size());
					nFile::readBigEndian(FileIn, uwCrc);
					if(uwCrc == 0) {
						// I want std::format so bad :(
						std::stringstream ss;
						ss << "Empty CRC near pos " << std::hex << FileIn.tellg();
						throw std::runtime_error(ss.str());
					}
					m_vProgramData.push_back(vRow);
					FileIn.seekg(DummyBytesCount, std::ios::cur);
				}
			} break;
			case tBitstreamCmd::tId::ISC_PROGRAM_USERCODE:
				// Read usercode, skip CRC
				nFile::readData(FileIn, m_UserCode.data(), m_UserCode.size());
				FileIn.seekg(2, std::ios::cur);
				break;
			case tBitstreamCmd::tId::ISC_PROGRAM_DONE:
				break;
			default: {
				// I want std::format so bad :(
				std::stringstream ss;
				ss << "Unhandled bitstream cmd near file pos 0x" << std::hex <<
					FileIn.tellg() << ": 0x" << std::setw(2) << std::setfill('0') <<
					int(Cmd.m_eId);
				throw std::runtime_error(ss.str());
			}
		}
	}
}

tBitstreamCmd::tBitstreamCmd(std::ifstream &FileIn)
{
	uint32_t ulCmdRaw;
	nFile::readBigEndian(FileIn, ulCmdRaw);
	m_eId = tBitstreamCmd::tId(ulCmdRaw >> 24);
	m_Operands[0] = (ulCmdRaw >> 16) & 0xFF;
	m_Operands[1] = (ulCmdRaw >> 8) & 0xFF;
	m_Operands[2] = (ulCmdRaw >> 0) & 0xFF;
}
