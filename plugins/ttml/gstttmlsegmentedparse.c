/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <libxml/parser.h>
#include <libxml/xmlwriter.h>
#include <gst/gstconfig.h>

#include "gstttmlbase.h"
#include "gstttmlsegmentedparse.h"
#include "gstttmlstate.h"
#include "gstttmltype.h"
#include "gstttmlspan.h"
#include "gstttmlevent.h"
#include "gstttmlattribute.h"
#include "gstttmlutils.h"
#include "gstttmltype.h"
#include "gstttmlnamespace.h"

GST_DEBUG_CATEGORY_EXTERN (ttmlsegmentedparse_debug);
#define GST_CAT_DEFAULT ttmlsegmentedparse_debug

#define LIBXML_CHAR (const guchar *)

static GstStaticPadTemplate ttmlparse_src_template =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS (TTML_MIME ", segmented = (boolean) TRUE"));

G_DEFINE_TYPE (
    GstTTMLSegmentedParse, gst_ttmlsegmentedparse, GST_TYPE_TTMLBASE);
#define parent_class gst_ttmlsegmentedparse_parent_class

static void
gst_ttmlsegmentedparse_attr_dump (
    GstTTMLAttribute *attr, xmlTextWriterPtr writer)
{
  gchar *attr_val;
  const gchar *attr_name = NULL;

  switch (attr->type) {
    case GST_TTML_ATTR_REGION:
      break;
    default:
      attr_val = gst_ttml_attribute_dump (attr);
      attr_name = gst_ttml_utils_enum_name (attr->type, AttributeType);
      if (attr_val && attr_name) {
        xmlTextWriterWriteAttribute (
            writer, LIBXML_CHAR attr_name, LIBXML_CHAR attr_val);
      }
      g_free (attr_val);
      break;
  }
}

static void
gst_ttmlsegmentedparse_paragraph_attr_dump (
    GstTTMLAttribute *attr, xmlTextWriterPtr writer)
{
  gchar *attr_val;
  const gchar *attr_name = NULL;

  switch (attr->type) {
    case GST_TTML_ATTR_REGION:
      attr_val = gst_ttml_attribute_dump (attr);
      attr_name = gst_ttml_utils_enum_name (attr->type, AttributeType);
      if (attr_val && attr_name) {
        xmlTextWriterWriteAttribute (
            writer, LIBXML_CHAR attr_name, LIBXML_CHAR attr_val);
      }
      g_free (attr_val);
      break;
    default:
      break;
  }
}

static void
gst_ttmlsegmentedparse_spans_dump (GstTTMLBase *base, xmlTextWriterPtr writer,
    GstClockTime ts, GstClockTime duration)
{
  GList *l;
  gboolean open = FALSE;
  gchar *end = gst_ttml_attribute_dump_time_expression (ts + duration);
  gchar *begin = gst_ttml_attribute_dump_time_expression (ts);

  /* for each span until a \n create a new <p> node */
  for (l = base->active_spans; l; l = l->next) {
    GstTTMLSpan *span = l->data;
    int chars_left = span->length;
    int frag_len;
    gchar *frag_start = span->chars; /* NOT NULL-terminated! */
    gchar *line_break = NULL;

    /* On parsing we merged paragraphs and spans. Now we have to
     * split again on newlines */
    do {
      line_break = g_utf8_strchr (frag_start, chars_left, '\n');
      frag_len = line_break ? line_break - frag_start : chars_left;

      if (!open) {
        open = TRUE;
        /* <p> */
        xmlTextWriterStartElement (writer, LIBXML_CHAR "p");
        xmlTextWriterWriteAttribute (
            writer, LIBXML_CHAR "begin", LIBXML_CHAR begin);
        xmlTextWriterWriteAttribute (
            writer, LIBXML_CHAR "end", LIBXML_CHAR end);
        g_list_foreach (span->style.attributes,
            (GFunc) gst_ttmlsegmentedparse_paragraph_attr_dump, writer);
      }

      if (frag_len) {
        /* <span> */
        xmlTextWriterStartElement (writer, LIBXML_CHAR "span");
        g_list_foreach (span->style.attributes,
            (GFunc) gst_ttmlsegmentedparse_attr_dump, writer);
        xmlTextWriterWriteFormatString (writer, "%.*s", frag_len, frag_start);
        /* </span> */
        xmlTextWriterEndElement (writer);
      }

      /* </p> */
      if (line_break) {
        xmlTextWriterEndElement (writer);
        open = FALSE;
        frag_len++;
      }

      frag_start += frag_len;
      chars_left -= frag_len;
    } while (chars_left);
  }

  /* </p> */
  if (open) {
    xmlTextWriterEndElement (writer);
  }

  g_free (end);
  g_free (begin);
}

static void
gst_ttmlsegmentedparse_region_dump (
    gchar *id, GList *attrs, xmlTextWriterPtr writer)
{
  /* <region> */
  xmlTextWriterStartElement (writer, LIBXML_CHAR "region");
  xmlTextWriterWriteAttribute (writer, LIBXML_CHAR "xml:id", LIBXML_CHAR id);
  g_list_foreach (attrs, (GFunc) gst_ttmlsegmentedparse_attr_dump, writer);
  /* </region> */
  xmlTextWriterEndElement (writer);
}

static void
gst_ttmlsegmentedparse_style_dump (
    gchar *id, GList *attrs, xmlTextWriterPtr writer)
{
  /* <style> */
  xmlTextWriterStartElement (writer, LIBXML_CHAR "style");
  xmlTextWriterWriteAttribute (writer, LIBXML_CHAR "xml:id", LIBXML_CHAR id);
  g_list_foreach (attrs, (GFunc) gst_ttmlsegmentedparse_attr_dump, writer);
  /* </style> */
  xmlTextWriterEndElement (writer);
}

static void
gst_ttmlsegmentedparse_namespace_dump (
    GstTTMLNamespace *ns, xmlTextWriterPtr writer)
{
  char name[0x100];
  strcpy (name, "xmlns");
  if (ns->name) {
    strcat (name, ":");
    strncat (name, ns->name, 0xF0);
  }
  xmlTextWriterWriteAttribute (
      writer, LIBXML_CHAR name, LIBXML_CHAR ns->value);
}

static GstBuffer *
gst_ttmlsegmentedparse_gen_buffer (
    GstTTMLBase *base, GstClockTime ts, GstClockTime duration)
{
  GstBuffer *buffer = NULL;
  GstMapInfo map_info;
  xmlBufferPtr buf;
  xmlTextWriterPtr writer;
  GstTTMLAttribute *attr;

  GST_DEBUG_OBJECT (base,
      "Generating buffer at %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT,
      GST_TIME_ARGS (ts), GST_TIME_ARGS (duration));

  buf = xmlBufferCreate ();
  writer = xmlNewTextWriterMemory (buf, 0);
  /* <?xml version="1.0" encoding="utf-8"?> */
  xmlTextWriterStartDocument (writer, NULL, "utf-8", NULL);
  /* <tt xmlns="http://www.w3.org/ns/ttml" /> */
  xmlTextWriterStartElement (writer, LIBXML_CHAR "tt");
  g_list_foreach (
      base->namespaces, (GFunc) gst_ttmlsegmentedparse_namespace_dump, writer);
  xmlTextWriterWriteAttribute (
      writer, LIBXML_CHAR "space", LIBXML_CHAR "preserve");
  attr = gst_ttml_state_get_attribute (
      &base->state, GST_TTML_ATTR_CELLRESOLUTION);
  if (attr) {
    gchar *value = gst_ttml_attribute_dump (attr);
    xmlTextWriterWriteAttribute (
        writer, LIBXML_CHAR "cellResolution", LIBXML_CHAR value);
    g_free (value);
    g_free (attr);
  }

  /* <head> */
  xmlTextWriterStartElement (writer, LIBXML_CHAR "head");

  /* create every style */
  if (base->state.saved_styling_attr_stacks) {
    /* <styling> */
    xmlTextWriterStartElement (writer, LIBXML_CHAR "styling");
    g_hash_table_foreach (base->state.saved_styling_attr_stacks,
        (GHFunc) gst_ttmlsegmentedparse_style_dump, writer);
    /* </styling> */
    xmlTextWriterEndElement (writer);
  }

  /* create every layout */
  if (base->state.saved_region_attr_stacks) {
    /* <layout> */
    xmlTextWriterStartElement (writer, LIBXML_CHAR "layout");
    g_hash_table_foreach (base->state.saved_region_attr_stacks,
        (GHFunc) gst_ttmlsegmentedparse_region_dump, writer);
    /* </layout> */
    xmlTextWriterEndElement (writer);
  }

  /* </head> */
  xmlTextWriterEndElement (writer);
  /* <body> */
  xmlTextWriterStartElement (writer, LIBXML_CHAR "body");
  gst_ttmlsegmentedparse_spans_dump (base, writer, ts, duration);
  /* </body> */
  xmlTextWriterEndElement (writer);
  /* </tt> */
  xmlTextWriterEndElement (writer);

  xmlTextWriterEndDocument (writer);
  xmlFreeTextWriter (writer);

  GST_MEMDUMP ("TTML:", buf->content, buf->use);

  /* FIXME we copy for now, we can use the buffer directly
   * but we need it to do for 0.10 and 1.0 */
  buffer = gst_buffer_new_and_alloc (buf->use);
  gst_buffer_map (buffer, &map_info, GST_MAP_WRITE);
  memcpy (map_info.data, buf->content, buf->use);
  gst_buffer_unmap (buffer, &map_info);

  xmlBufferFree (buf);

  return buffer;
}

static void
gst_ttmlsegmentedparse_class_init (GstTTMLSegmentedParseClass *klass)
{
  GstTTMLBaseClass *base_klass = GST_TTMLBASE_CLASS (klass);

  parent_class = GST_TTMLBASE_CLASS (g_type_class_peek_parent (klass));

  /* Here we register a Pad Template called "src" which the base class will
   * use to instantiate the src pad. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_static_pad_template_get (&ttmlparse_src_template));

  gst_element_class_set_details_simple (GST_ELEMENT_CLASS (klass),
      "TTML subtitle parser", "Codec/Parser/Subtitle",
      "Parse TTML subtitle streams into a segmented TTML stream",
      "Fluendo S.A. <support@fluendo.com>");

  base_klass->gen_buffer =
      GST_DEBUG_FUNCPTR (gst_ttmlsegmentedparse_gen_buffer);
}

static void
gst_ttmlsegmentedparse_init (GstTTMLSegmentedParse *parse)
{
}
