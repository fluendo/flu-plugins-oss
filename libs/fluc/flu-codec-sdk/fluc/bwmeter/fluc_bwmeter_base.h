/*
 * Copyright 2021 FLUENDO S.A.
 * SPDX-License-Identifier: LGPL-2.1-only
 */



#ifndef _FLUC_BWMETER_BASE_H_
#define _FLUC_BWMETER_BASE_H_

#include "fluc_bwmeter_private.h"

G_BEGIN_DECLS

void fluc_bwmeter_base_delete (FlucBwMeter *meter);
void fluc_bwmeter_base_init (FlucBwMeter *meter);
void fluc_bwmeter_base_dispose (FlucBwMeter *meter);
void fluc_bwmeter_base_start (FlucBwMeter *meter);
void fluc_bwmeter_base_end (FlucBwMeter *meter);
void fluc_bwmeter_base_update (FlucBwMeter *meter);
void fluc_bwmeter_base_data (FlucBwMeter *meter);
void fluc_bwmeter_base_compute (FlucBwMeter *meter, gint64 time);

G_END_DECLS
#endif /* _FLUC_BWMETER_BASE_H_ */
