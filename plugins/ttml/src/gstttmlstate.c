/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstttmlattribute.h"
#include "gstttmlstate.h"
#include "gstttmlutils.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

/* Set the state to default values */
void
gst_ttml_state_reset (GstTTMLState *state)
{
  state->last_span_id = 0;
  state->begin = GST_CLOCK_TIME_NONE;
  state->end = GST_CLOCK_TIME_NONE;
  state->container_begin = GST_CLOCK_TIME_NONE;
  state->container_end = GST_CLOCK_TIME_NONE;
  state->tick_rate = 1.0 / GST_SECOND;
  state->frame_rate = 30.0;
  state->frame_rate_num = 1;
  state->frame_rate_den = 1;
  state->whitespace_preserve = FALSE;
  state->sequential_time_container = FALSE;
  if (state->history) {
    GST_WARNING ("Attribute stack should have been empty");
    g_list_free_full (state->history,
        (GDestroyNotify)gst_ttml_attribute_free);
    state->history = NULL;
  }
}

/* Puts the given GstTTMLAttribute into the state, overwritting the current
 * value. Normally you would use gst_ttmlparse_state_push_attribute() to
 * store the current value into an attribute stack before overwritting it. */
void
gst_ttml_state_set_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr)
{
  switch (attr->type) {
    case GST_TTML_ATTR_NODE_TYPE:
      state->node_type = attr->value.node_type;
      break;
    case GST_TTML_ATTR_BEGIN:
      state->begin = attr->value.time;
      break;
    case GST_TTML_ATTR_END:
      state->end = attr->value.time;
      break;
    case GST_TTML_ATTR_DUR:
      state->end = state->begin + attr->value.time;
      break;
    case GST_TTML_ATTR_TICK_RATE:
      state->tick_rate = attr->value.d;
      break;
    case GST_TTML_ATTR_FRAME_RATE:
      state->frame_rate = attr->value.d;
      break;
    case GST_TTML_ATTR_FRAME_RATE_MULTIPLIER:
      state->frame_rate_num = attr->value.num;
      state->frame_rate_den = attr->value.den;
      break;
    case GST_TTML_ATTR_WHITESPACE_PRESERVE:
      state->whitespace_preserve = attr->value.b;
      break;
    case GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER:
      state->sequential_time_container = attr->value.b;
      break;
    default:
      GST_DEBUG ("Unknown attribute type %d", attr->type);
      break;
  }
}

/* MERGES the given GstTTMLAttribute into the state. The effect of the merge
 * depends on the type of attribute. By default it calls the _set_ method
 * above. */
void
gst_ttml_state_merge_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr)
{
  switch (attr->type) {
    case GST_TTML_ATTR_BEGIN:
      state->begin = attr->value.time;
      if (GST_CLOCK_TIME_IS_VALID (state->container_begin))
        state->begin += state->container_begin;
      break;
    case GST_TTML_ATTR_END:
      state->end = attr->value.time;
      if (GST_CLOCK_TIME_IS_VALID (state->container_begin))
        state->end += state->container_begin;
      if (GST_CLOCK_TIME_IS_VALID (state->container_end))
        state->end = MIN (state->end, state->container_end);
      break;
    case GST_TTML_ATTR_DUR:
      state->end = attr->value.time;
      if (GST_CLOCK_TIME_IS_VALID (state->begin))
        state->end += state->begin;
      if (GST_CLOCK_TIME_IS_VALID (state->container_end))
        state->end = MIN (state->end, state->container_end);
      break;
    default:
      gst_ttml_state_set_attribute (state, attr);
      break;
  }
}

/* Read from the state an attribute specified in attr->type and store it in
 * attr->value */
void
gst_ttml_state_get_attribute (GstTTMLState *state,
    GstTTMLAttribute *attr)
{
  switch (attr->type) {
    case GST_TTML_ATTR_NODE_TYPE:
      attr->value.node_type = state->node_type;
      break;
    case GST_TTML_ATTR_BEGIN:
      attr->value.time = state->begin;
      break;
    case GST_TTML_ATTR_END:
      attr->value.time = state->end;
      break;
    case GST_TTML_ATTR_DUR:
      attr->value.time = state->end - state->begin;
      break;
    case GST_TTML_ATTR_TICK_RATE:
      attr->value.d = state->tick_rate;
      break;
    case GST_TTML_ATTR_FRAME_RATE:
      attr->value.d = state->frame_rate;
      break;
    case GST_TTML_ATTR_FRAME_RATE_MULTIPLIER:
      attr->value.num = state->frame_rate_num;
      attr->value.den = state->frame_rate_den;
      break;
    case GST_TTML_ATTR_WHITESPACE_PRESERVE:
      attr->value.b = state->whitespace_preserve;
      break;
    case GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER:
      attr->value.b = state->sequential_time_container;
      break;
    default:
      GST_DEBUG ("Unknown attribute type %d", attr->type);
      return;
  }
}

/* Puts the passed-in attribute into the state, and pushes the previous value
 * into the attribute stack, for later retrieval.
 * The GstTTMLAttribute now belongs to the stack, do not free! */
void
gst_ttml_state_push_attribute (GstTTMLState *state,
    GstTTMLAttribute *new_attr)
{
  GstTTMLAttribute *old_attr = g_new (GstTTMLAttribute, 1);
  old_attr->type = new_attr->type;
  gst_ttml_state_get_attribute (state, old_attr);
  state->history = g_list_prepend (state->history, old_attr);
  gst_ttml_state_merge_attribute (state, new_attr);
  gst_ttml_attribute_free (new_attr);

  GST_LOG ("Pushed attribute %p (type %d)", old_attr,
      old_attr==NULL?-1:old_attr->type);
}

/* Pops an attribute from the stack and puts in the state, overwritting the
 * current value */
GstTTMLAttributeType
gst_ttml_state_pop_attribute (GstTTMLState *state)
{
  GstTTMLAttribute *attr;
  GstTTMLAttributeType type;

  if (!state->history) {
    GST_ERROR ("Unable to pop attribute: empty stack");
  }
  attr  = (GstTTMLAttribute *)state->history->data;
  type = attr->type;
  state->history = g_list_delete_link (state->history, state->history);

  GST_LOG ("Popped attribute %p (type %d)", attr, attr==NULL?-1:type);

  gst_ttml_state_set_attribute (state, attr);

  gst_ttml_attribute_free (attr);

  return type;
}

