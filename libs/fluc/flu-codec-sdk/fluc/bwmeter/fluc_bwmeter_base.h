#ifndef FLUC_BWMETER_BASE_H
#define FLUC_BWMETER_BASE_H

#include "fluc_bwmeter_private.h"

G_BEGIN_DECLS

void fluc_bwmeter_base_delete (FlucBwMeter * meter);
void fluc_bwmeter_base_init (FlucBwMeter * meter);
void fluc_bwmeter_base_dispose (FlucBwMeter * meter);
void fluc_bwmeter_base_start (FlucBwMeter * meter);
void fluc_bwmeter_base_end (FlucBwMeter * meter);
void fluc_bwmeter_base_update (FlucBwMeter * meter);
void fluc_bwmeter_base_data (FlucBwMeter * meter);
void fluc_bwmeter_base_compute (FlucBwMeter * meter, gint64 time);

G_END_DECLS

#endif /* FLUC_BWMETER_BASE_H */

