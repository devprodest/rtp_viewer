#include "source_factory.h"

#include "source_file.h"
#include "source_rtp.h"

source_s * source_create(const source_params_s * params)
{
    if (params->path)
    {
        return source_file_create();
    }
    return source_rtp_create();
}
