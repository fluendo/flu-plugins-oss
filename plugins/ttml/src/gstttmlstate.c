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

GST_DEBUG_CATEGORY_EXTERN (ttmlparse_debug);
#define GST_CAT_DEFAULT ttmlparse_debug

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
  state->id = NULL;
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

  gst_ttml_style_reset (&state->style);

  if (state->attribute_stack) {
    GST_WARNING ("Attribute stack should have been empty");
    gst_ttml_state_free_attr_stack (state->attribute_stack);
    state->attribute_stack = NULL;
  }

  if (state->saved_attr_stacks) {
    g_hash_table_unref (state->saved_attr_stacks);
    state->saved_attr_stacks = NULL;
  }
}

/* Puts the given GstTTMLAttribute into the state, overwritting the current
 * value. Normally you would use gst_ttmlparse_state_push_attribute() to
 * store the current value into an attribute stack before overwritting it. */
static void
gst_ttml_state_set_attribute (GstTTMLState *state,
    const GstTTMLAttribute *attr)
{
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
      state->frame_rate_num = attr->value.num;
      state->frame_rate_den = attr->value.den;
      break;
    case GST_TTML_ATTR_WHITESPACE_PRESERVE:
      state->whitespace_preserve = attr->value.b;
      break;
    case GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER:
      state->sequential_time_container = attr->value.b;
      break;
    case GST_TTML_ATTR_STYLE:
      gst_ttml_state_restore_attr_stack (state, attr->value.string);
      break;
    case GST_TTML_ATTR_COLOR:
      state->style.color = attr->value.color;
      break;
    case GST_TTML_ATTR_BACKGROUND_COLOR:
      state->style.background_color = attr->value.color;
      break;
    case GST_TTML_ATTR_DISPLAY:
      state->style.display = attr->value.b;
      break;
    case GST_TTML_ATTR_FONT_FAMILY:
      if (state->style.font_family)
        g_free (state->style.font_family);
      state->style.font_family = g_strdup (attr->value.string);
      break;
    case GST_TTML_ATTR_FONT_STYLE:
      state->style.font_style = attr->value.font_style;
      break;
    case GST_TTML_ATTR_FONT_WEIGHT:
      state->style.font_weight = attr->value.font_weight;
      break;
    default:
      GST_DEBUG ("Unknown attribute type %d", attr->type);
      break;
  }
}

/* MERGES the given GstTTMLAttribute into the state. The effect of the merge
 * depends on the type of attribute. By default it calls the _set_ method
 * above. */
static void
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
static void
gst_ttml_state_get_attribute (GstTTMLState *state,
    GstTTMLAttribute *attr)
{
  switch (attr->type) {
    case GST_TTML_ATTR_NODE_TYPE:
      attr->value.node_type = state->node_type;
      break;
    case GST_TTML_ATTR_ID:
      attr->value.string = g_strdup (state->id);
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
    case GST_TTML_ATTR_STYLE:
      /* Nothing to do here: The style attribute is expanded into multiple
       * other attributes when set. */
      attr->value.string = NULL;
      break;
    case GST_TTML_ATTR_COLOR:
      attr->value.color = state->style.color;
      break;
    case GST_TTML_ATTR_BACKGROUND_COLOR:
      attr->value.color = state->style.background_color;
      break;
    case GST_TTML_ATTR_DISPLAY:
      attr->value.b = state->style.display;
      break;
    case GST_TTML_ATTR_FONT_FAMILY:
      attr->value.string = g_strdup (state->style.font_family);
      break;
    case GST_TTML_ATTR_FONT_STYLE:
      attr->value.font_style = state->style.font_style;
      break;
    case GST_TTML_ATTR_FONT_WEIGHT:
      attr->value.font_weight = state->style.font_weight;
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
  state->attribute_stack = g_list_prepend (state->attribute_stack, old_attr);
  gst_ttml_state_merge_attribute (state, new_attr);
  gst_ttml_attribute_free (new_attr);

  GST_LOG ("Pushed attribute 0x%p (type %s)", old_attr,
      gst_ttml_attribute_type_name (old_attr->type));
}

/* Pops an attribute from the stack and puts in the state, overwritting the
 * current value */
GstTTMLAttributeType
gst_ttml_state_pop_attribute (GstTTMLState *state)
{
  GstTTMLAttribute *attr;
  GstTTMLAttributeType type;

  if (!state->attribute_stack) {
    GST_ERROR ("Unable to pop attribute: empty stack");
  }
  attr  = (GstTTMLAttribute *)state->attribute_stack->data;
  type = attr->type;
  state->attribute_stack = g_list_delete_link (state->attribute_stack,
      state->attribute_stack);

  GST_LOG ("Popped attribute 0x%p (type %s)", attr,
      gst_ttml_attribute_type_name (type));

  gst_ttml_state_set_attribute (state, attr);

  gst_ttml_attribute_free (attr);

  return type;
}

/* Create a copy of the current attribute stack and store it in a hash table
 * with the specified ID string.
 * Create the hash table if necessary. */
void
gst_ttml_state_save_attr_stack (GstTTMLState *state, const gchar *id)
{
  GList *attr_link = state->attribute_stack;
  GList *attr_stack_copy = NULL;
  gchar *id_copy;

  if (!state->saved_attr_stacks) {
    state->saved_attr_stacks = g_hash_table_new_full (
        g_str_hash, g_str_equal, g_free,
        (GDestroyNotify)gst_ttml_state_free_attr_stack);
  }

  while (attr_link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *)attr_link->data;
    if (attr->type > GST_TTML_ATTR_STYLE) {
      GstTTMLAttribute *attr_copy = g_new (GstTTMLAttribute, 1);
      attr_copy->type = attr->type;
      gst_ttml_state_get_attribute (state, attr_copy);
      /* This must be appended, not prepended, to preserve the list direction.
       * If the operation turns out to be too lengthy (because append must
       * traverse the whole list) alternatives can be found (like keeping
       * a pointer to the last link), although I do not think there will
       * ever be that many attributes in a style... */
      attr_stack_copy = g_list_append (attr_stack_copy, attr_copy);
    }

    attr_link = attr_link->next;
  }

  id_copy = g_strdup (id);

  GST_DEBUG ("Storing style '%s'", id);

  g_hash_table_insert (state->saved_attr_stacks, id_copy, attr_stack_copy);
}

void
gst_ttml_state_restore_attr_stack (GstTTMLState *state, const gchar *id)
{
  GList *attr_link;

  /* When a Style attribute is found, the previous style is pushed onto the
   * stack. However "style" is not a member of the state, so a NULL attr
   * is actually pushed. Here we filter out this kind of styles.
   */
  if (!id) return;

  if (!state->saved_attr_stacks ||
      !g_hash_table_contains (state->saved_attr_stacks, id)) {
    GST_WARNING ("Undefined style '%s'", id);
    return;
  }

  attr_link = (GList *)g_hash_table_lookup (state->saved_attr_stacks, id);

  GST_DEBUG ("Applying style '%s'", id);

  while (attr_link) {
    GstTTMLAttribute *attr = (GstTTMLAttribute *)attr_link->data;
    if (attr->type > GST_TTML_ATTR_STYLE) {
      GstTTMLAttribute *attr_copy = gst_ttml_attribute_copy (attr);
      gst_ttml_state_push_attribute (state, attr_copy);
    }

    attr_link = attr_link->next;
  }
}