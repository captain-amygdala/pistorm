/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _FILE_HPP_
#define _FILE_HPP_

#include <fstream>
#include "endian.hpp"

namespace nFile {

template<typename t_tStream, typename t_tData>
t_tStream readLittleEndian(t_tStream &File, t_tData &Data)
{
	File.read(reinterpret_cast<char*>(&Data), sizeof(Data));
	Data = nEndian::littleToNative(Data);
}

template<typename t_tStream, typename t_tData>
void readBigEndian(t_tStream &File, t_tData &Data)
{
	File.read(reinterpret_cast<char*>(&Data), sizeof(Data));
	Data = nEndian::bigToNative(Data);
}

template<typename t_tStream, typename t_tData>
void readData(t_tStream &File, t_tData *pData, size_t ElementCount)
{
	File.read(reinterpret_cast<char*>(pData), ElementCount * sizeof(*pData));
}

} // namespace nFile

#endif // _FILE_HPP_
