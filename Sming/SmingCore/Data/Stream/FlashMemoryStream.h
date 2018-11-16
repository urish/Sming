/****
 * Sming Framework Project - Open Source framework for high efficiency native ESP8266 development.
 * Created 2015 by Skurydin Alexey
 * http://github.com/anakod/Sming
 * All files of the Sming Core are provided under the LGPL v3 license.
 *
 * @author: 23 Oct 2018 - mikee47 <mike@sillyhouse.net>
 *
 ****/

#ifndef _SMING_CORE_DATA_FLASH_MEMORY_STREAM_H_
#define _SMING_CORE_DATA_FLASH_MEMORY_STREAM_H_

#include "DataSourceStream.h"

/** @addtogroup stream
 *  @{
 */

/*
 * FlashMemoryDataStream
 *
 * Provides a stream buffer on flash storage, so it's read-only
 *
 */
class FlashMemoryStream : public IDataSourceStream
{
public:
	FlashMemoryStream(const FlashString& flashString) : flashString(flashString)
	{
	}

	virtual StreamType getStreamType() const
	{
		return eSST_Memory;
	}

	/**
	 * @brief Return the total length of the stream
	 * @retval int -1 is returned when the size cannot be determined
	*/
	int available()
	{
		return flashString.length() - readPos;
	}

	virtual uint16_t readMemoryBlock(char* data, int bufSize);

	virtual bool seek(int len);

	virtual bool isFinished()
	{
		return readPos >= flashString.length();
	}

private:
	const FlashString& flashString;
	size_t readPos = 0;
};

/** @} */
#endif /* _SMING_CORE_DATA_FLASH_MEMORY_STREAM_H_ */
