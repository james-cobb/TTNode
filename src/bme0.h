// Copyright 2017 Inca Roads LLC.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#ifndef BME280_0__
#define BME280_0__

#ifdef bme
#undef bme
#endif
#define bme(FUNC) s_bme280_0_##FUNC

#include "bme-h.h"

#endif
