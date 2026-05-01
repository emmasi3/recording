#include "core/MediaTypes.h"

extern "C"
{
#include <libavutil/error.h>
}

namespace streamer
{
	int AVStrError::strerror(int errnum, char* errbuf, size_t errbuf_size)
	{
		return av_strerror(errnum, errbuf, errbuf_size);
	}

	size_t AVStrError::maxErrorStringSize() noexcept
	{
		return static_cast<size_t>(AV_ERROR_MAX_STRING_SIZE);
	}
}