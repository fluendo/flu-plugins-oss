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
#include "gstttmlstyle.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlbase_debug);
#define GST_CAT_DEFAULT ttmlbase_debug

/* Free internally allocated memory for the state */
static void
gst_ttml_state_free_content (GstTTMLState *state)
{
  g_free (state->id);
}

/* Free this state structure and its contents */
void
gst_ttml_state_free (GstTTMLState *state)
{
  gst_ttml_state_free_content (state);
  g_free (state);
}

static void
gst_ttml_state_free_attr_stack (GList *stack)
{
  g_list_free_full (stack, (GDestroyNotify)gst_ttml_attribute_free);
}

/* Set the state to default values */
void
gst_ttml_state_reset (GstTTMLState *state)
{
  gst_ttml_state_free_content (state);

  state->last_span_id = 0;
  state->last_zindex_micro = 0;
  state->id = NULL;
  state->begin = GST_CLOCK_TIME_NONE;
  state->end = GST_CLOCK_TIME_NONE;
  state->container_begin = GST_CLOCK_TIME_NONE;
  state->container_end = GST_CLOCK_TIME_NONE;
  state->tick_rate = 1.0 / GST_SECOND;
  state->frame_rate = 30.0;
  state->frame_rate_num = 1;
  state->frame_rate_den = 1;
  state->cell_resolution_x = 32;
  state->cell_resolution_y = 15;
  state->whitespace_preserve = FALSE;
  state->sequential_time_container = FALSE;

  gst_ttml_style_reset (&state->style);

  if (state->attribute_stack) {
    GST_WARNING ("Attribute stack should have been empty");
    gst_ttml_state_free_attr_stack (state->attribute_stack);
    state->attribute_stack = NULL;
  }

  if (state->saved_styling_attr_stacks) {
    g_hash_table_unref (state->saved_styling_attr_stacks);
    state->saved_styling_attr_stacks = NULL;
  }

  if (state->saved_region_attr_stacks) {
    g_hash_table_unref (state->saved_region_attr_stacks);
    state->saved_region_attr_stacks = NULL;
  }

  if (state->saved_data) {
    g_hash_table_unref (state->saved_data);
    state->saved_data = NULL;
  }
}

/* Puts the given GstTTMLAttribute into the state, overwritting the current
 * value. Normally you would use gst_ttmlparse_state_push_attribute() to
 * store the current value into an attribute stack before overwritting it.
 * The overwritten current value (if any) is returned. Do not forget to free
 * it! */
static GstTTMLAttribute *
gst_ttml_state_set_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr)
{
  GstTTMLAttribute *ret_attr = NULL;

  switch (attr->type) {
    case GST_TTML_ATTR_NODE_TYPE:
      state->node_type = attr->value.node_type;
      break;
    case GST_TTML_ATTR_ID:
      if (state->id)
        g_free (state->id);
      state->id = g_strdup (attr->value.string);
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
      state->frame_rate_num = attr->value.fraction.num;
      state->frame_rate_den = attr->value.fraction.den;
      break;
    case GST_TTML_ATTR_CELLRESOLUTION:
      state->cell_resolution_x = (int)attr->value.length[0].f;
      state->cell_resolution_y = (int)attr->value.length[1].f;
      break;
    case GST_TTML_ATTR_WHITESPACE_PRESERVE:
      state->whitespace_preserve = attr->value.b;
      break;
    case GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER:
      state->sequential_time_container = attr->value.b;
      break;
    case GST_TTML_ATTR_STYLE:
      gst_ttml_state_restore_attr_stack (state,
          state->saved_styling_attr_stacks, attr->value.string);
      break;
    default:
      /* All Styling attributes are handled here */
      ret_attr = gst_ttml_style_set_attr (&state->style, attr);
      break;
  }

  return ret_attr;
}

/* MERGES the given GstTTMLAttribute into the state. The effect of the merge
 * depends on the type of attribute. By default it calls the _set_ method
 * above. */
static void
gst_ttml_state_merge_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr)
{
  GstTTMLAttribute *prev_attr;

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
      prev_attr = gst_ttml_state_set_attribute (state, attr);
      if (prev_attr)
        gst_ttml_attribute_free (prev_attr);
      break;
  }
}

/* Read from the state the attribute specified by type and return a new
 * attribute */
static GstTTMLAttribute *
gst_ttml_state_get_attribute (GstTTMLState *state, GstTTMLAttributeType type)
{
  const GstTTMLAttribute *curr_attr;
  GstTTMLAttribute *attr;

  switch (type) {
    case GST_TTML_ATTR_NODE_TYPE:
      attr = gst_ttml_attribute_new_node (state->node_type);
      break;
    case GST_TTML_ATTR_ID:
      attr = gst_ttml_attribute_new_string (type, state->id);
      break;
    case GST_TTML_ATTR_BEGIN:
      attr = gst_ttml_attribute_new_time (type, state->begin);
      break;
    case GST_TTML_ATTR_END:
      attr = gst_ttml_attribute_new_time (type, state->end);
      break;
    case GST_TTML_ATTR_DUR:
      attr = gst_ttml_attribute_new_time (type, state->end - state->begin);
      break;
    case GST_TTML_ATTR_TICK_RATE:
      attr = gst_ttml_attribute_new_double (type, state->tick_rate);
      break;
    case GST_TTML_ATTR_FRAME_RATE:
      attr = gst_ttml_attribute_new_double (type, state->frame_rate);
      break;
    case GST_TTML_ATTR_FRAME_RATE_MULTIPLIER:
      attr = gst_ttml_attribute_new_fraction (type, state->frame_rate_den,
          state->frame_rate_den);
      break;
    case GST_TTML_ATTR_CELLRESOLUTION:
      attr = g_new0 (GstTTMLAttribute, 1);
      attr->type = type;
      attr->value.length[0].f = (float)state->cell_resolution_x;
      attr->value.length[0].unit = GST_TTML_LENGTH_UNIT_CELLS;
      attr->value.length[1].f = (float)state->cell_resolution_y;
      attr->value.length[1].unit = GST_TTML_LENGTH_UNIT_CELLS;
      break;
    case GST_TTML_ATTR_WHITESPACE_PRESERVE:
      attr = gst_ttml_attribute_new_boolean (type, state->whitespace_preserve);
      break;
    case GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER:
      attr = gst_ttml_attribute_new_boolean (type,
          state->sequential_time_container);
      break;
    case GST_TTML_ATTR_STYLE:
      /* Nothing to do here: The style attribute is expanded
       * into multiple other attributes when set.
       * Same happens with the region attribute, but we want the region ID
       * to be stored. */
      attr = gst_ttml_attribute_new_string (type, NULL);
      break;
    default:
      /* All Styling attributes are handled here */
      curr_attr = gst_ttml_style_get_attr (&state->style, type);
      if (curr_attr) {
        attr = gst_ttml_attribute_copy (curr_attr, TRUE);
      } else {
        attr = gst_ttml_attribute_new_styling_default (type);
      }
      break;
  }

  return attr;
}

/* Puts the passed-in attribute into the state, and pushes the previous value
 * into the attribute stack, for later retrieval.
 * The GstTTMLAttribute now belongs to the stack, do not free! */
void
gst_ttml_state_push_attribute (GstTTMLState *state,
    GstTTMLAttribute *new_attr)
{
  GstTTMLAttribute *old_attr;

  if (new_attr->type == GST_TTML_ATTR_BACKGROUND_COLOR) {
    if (state->node_type == GST_TTML_NODE_TYPE_REGION ||
        state->node_type == GST_TTML_NODE_TYPE_DIV) {
      /* Special cases:
       *  - A backgroundColor attribute specified inside a REGION
       *    node actually means REGION background, not SPAN background.
       *  - Same for DIV nodes. Does not make much sense to me, but allows
       *    us to pass the Padding testsuite (which might be wrong, but
       *    other TTML renderers do it like this).
       */
      new_attr->type = GST_TTML_ATTR_BACKGROUND_REGION_COLOR;
    }
  }

  old_attr = gst_ttml_state_get_attribute (state, new_attr->type);

  state->attribute_stack = g_list_prepend (state->attribute_stack, old_attr);
  gst_ttml_state_merge_attribute (state, new_attr);
  gst_ttml_attribute_free (new_attr);

  GST_LOG ("Pushed attribute 0x%p (type %s)", old_attr,
      gst_ttml_utils_enum_name (old_attr->type, AttributeType));
}

/* Pops an attribute from the stack and puts in the state, overwritting the
 * current value, which is returned. Do not forget to free it! */
GstTTMLAttributeType
gst_ttml_state_pop_attribute (GstTTMLState *state,
    GstTTMLAttribute **prev_attr_ptr)
{
  GstTTMLAttribute *attr, *prev_attr;
  GstTTMLAttributeType type;

  if (!state->attribute_stack) {
    GST_ERROR ("Unable to pop attribute: empty stack");
  }
  attr  = (GstTTMLAttribute *)state->attribute_stack->data;
  type = attr->type;
  state->attribute_stack = g_list_delete_link (state->attribute_stack,
      state->attribute_stack);

  GST_LOG ("Popped attribute 0x%p (type %s)", attr,
      gst_ttml_utils_enum_name (type, AttributeType));

  prev_attr = gst_ttml_state_set_attribute (state, attr);
  if (prev_attr_ptr)
    *prev_attr_ptr = prev_attr;

  gst_ttml_attribute_free (attr);

  return type;
}

/* Create a copy of the current attribute stack and store it in a hash table
 * with the specified ID string.
 * Create the hash table if necessary. Used for referential and region styling.
 */
void
gst_ttml_state_save_attr_stack (GstTTMLState *state, GHashTable **table,
    const gchar *id)
{
  GList *attr_link = state->attribute_stack;
  GList *attr_stack_copy = NULL;

  if (!*table) {
    *table =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
        (GDestroyNotify)gst_ttml_state_free_attr_stack);
  }

  while (attr_link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *)attr_link->data;
    if (attr->type > GST_TTML_ATTR_STYLE) {
      GstTTMLAttribute *attr_copy =
          gst_ttml_state_get_attribute (state, attr->type);
      /* This must be appended, not prepended, to preserve the list direction.
       * If the operation turns out to be too lengthy (because append must
       * traverse the whole list) alternatives can be found (like keeping
       * a pointer to the last link), although I do not think there will
       * ever be that many attributes in a style... */
      attr_stack_copy = g_list_append (attr_stack_copy, attr_copy);
    }

    attr_link = attr_link->next;
  }

  if (attr_stack_copy) {
    gchar *id_copy = g_strdup (id);

    GST_DEBUG ("Storing style or region '%s'", id);
    g_hash_table_insert (*table, id_copy, attr_stack_copy);
  } else {
    GST_WARNING ("Trying to store empty style or region definition '%s'", id);
  }
}

/* Retrieve the attribute stack with the given id from the hash table and
 * apply it. Used for referential and region styling. */
void
gst_ttml_state_restore_attr_stack (GstTTMLState *state, GHashTable *table,
    const gchar *id)
{
  GList *attr_link = NULL;

  /* When a Style or Region attribute is found, the previous style or region
   * is pushed onto the stack.
   * However "style" and "region" are not members of the state, so a NULL attr
   * is actually pushed. Here we filter out this kind of attributes. */
  if (!id)
    return;

  if (table) {
    g_hash_table_lookup_extended (table, id, NULL, (gpointer *)&attr_link);
  }

  if (!attr_link) {
    GST_WARNING ("Undefined style or region '%s'", id);
    return;
  }

  GST_DEBUG ("Applying style or region '%s'", id);

  while (attr_link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *) attr_link->data;
    if (attr->type > GST_TTML_ATTR_STYLE) {
      GstTTMLAttribute *attr_copy = gst_ttml_attribute_copy (attr, TRUE);
      gst_ttml_state_push_attribute (state, attr_copy);
    }

    attr_link = attr_link->next;
  }
}

/* Store the current data in the saved_data hash table with the specified ID
 * string. Create the hash table if necessary. Data is fully transferred, do
 * not free.
 */
void
gst_ttml_state_save_data (GstTTMLState *state, guint8 *data, gint length,
    const gchar *id)
{
  gchar *id_copy = g_strdup (id);

  if (!state->saved_data) {
    state->saved_data =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  }
  
  /* Add a little header stating the data size */
  GST_DEBUG ("Storing image '%s' (raw length is %d bytes)", id, length);
  data = (guint8 *)g_realloc (data, length + 4);
  memmove (data + 4, data, length);
  *(gint32*)data = length;
  g_hash_table_insert (state->saved_data, id_copy, data);
}

void
gst_ttml_state_restore_data (const GstTTMLState *state, const gchar *id,
    guint8 **data, gint *length)
{
  guint8 * rawdata;

  *data = NULL;
  *length = 0;

  if (!id)
    return;

  if (!state->saved_data)
    return;

  rawdata = (guint8 *)g_hash_table_lookup (state->saved_data, id);

  if (rawdata) {
    *length = *(gint32*)rawdata;
    *data = rawdata + 4;
  }
}