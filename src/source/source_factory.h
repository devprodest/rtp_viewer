#pragma once

#include "source.h"

/// \brief Создаёт источник по параметрам: RTP, если path == nullptr, иначе файл.
source_s * source_create(const source_params_s * params);