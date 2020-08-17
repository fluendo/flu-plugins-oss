#include "fluc_bwmeter_sock.h"
#include "fluc_bwmeter_private.h"

static void
fluc_bwmeter_sock_data (FlucBwMeter * meter, guint32 size)
{
  fluc_rmutex_lock (&meter->lock);
  meter->state.bytes += size;
  fluc_bwmeter_base_data (meter);
  fluc_rmutex_unlock (&meter->lock);
}

FlucBwMeter *
fluc_bwmeter_sock_new ()
{
  FlucBwMeter *meter;
  meter = g_malloc0 (sizeof (FlucBwMeter));
  fluc_bwmeter_base_init (meter);
  meter->delete = fluc_bwmeter_base_delete;
  meter->start = fluc_bwmeter_base_start;
  meter->end = fluc_bwmeter_base_end;
  meter->update = fluc_bwmeter_base_update;
  meter->data = fluc_bwmeter_sock_data;
  return meter;
}
