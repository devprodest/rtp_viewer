#pragma once

#include "source.h"

/// \brief Создаёт источник RTP-over-UDP.
///
/// \details Возвращает указатель на статически выделенный source_s.
///          Не освобождать вручную — вызов source_close сбросит внутреннее
///          состояние, память переиспользуется.
source_s * source_rtp_create(void);