/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_UTILS_H__
#define __GST_TTML_UTILS_H__

#include <gst-compat.h>
#include "gstttmlforward.h"
#include "gstttmlenums.h"

G_BEGIN_DECLS

/* Enum token arrays, for parsing and writing debugging messages */
extern const GstTTMLToken *GstTTMLUtilsTokensNodeType;
extern const GstTTMLToken *GstTTMLUtilsTokensAttributeType;
extern const GstTTMLToken *GstTTMLUtilsTokensLengthUnit;
extern const GstTTMLToken *GstTTMLUtilsTokensFontStyle;
extern const GstTTMLToken *GstTTMLUtilsTokensFontWeight;
extern const GstTTMLToken *GstTTMLUtilsTokensTextDecoration;
extern const GstTTMLToken *GstTTMLUtilsTokensTextAlign;
extern const GstTTMLToken *GstTTMLUtilsTokensDisplayAlign;
extern const GstTTMLToken *GstTTMLUtilsTokensWrapOption;
extern const GstTTMLToken *GstTTMLUtilsTokensShowBackground;
extern const GstTTMLToken *GstTTMLUtilsTokensTimeBase;
extern const GstTTMLToken *GstTTMLUtilsTokensSMPTEImageType;
extern const GstTTMLToken *GstTTMLUtilsTokensSMPTEEncoding;

/* Enum name -> value */
#define gst_ttml_utils_enum_parse(name,type) \
  (GstTTML ## type)gst_ttml_utils_enum_parse_func (name, GstTTMLUtilsTokens ## type)

/* Enum value -> name (For debugging purposes) */
#define gst_ttml_utils_enum_name(val,type) \
  gst_ttml_utils_enum_name_func ((int)val, GstTTMLUtilsTokens ## type)

/* Flags list -> value (like enums, but allows merging multiple values in one line */
#define gst_ttml_utils_flags_parse(name,type) \
  (GstTTML ## type)gst_ttml_utils_flags_parse_func (name, GstTTMLUtilsTokens ## type)

/* Flags value -> list (For debugging purposes) */
#define gst_ttml_utils_flags_name(val,type) \
  gst_ttml_utils_flags_name_func ((int)val, GstTTMLUtilsTokens ## type)

/* Internal use. Required by the macros above. */
int gst_ttml_utils_enum_parse_func (const gchar *name, const GstTTMLToken *list);
const gchar *gst_ttml_utils_enum_name_func (int val, const GstTTMLToken *list);
int gst_ttml_utils_flags_parse_func (const gchar *name, const GstTTMLToken *list);
const gchar *gst_ttml_utils_flags_name_func (int val, const GstTTMLToken *list);

/* Enum GTypes to register properties */
#define GST_TTML_TEXT_ALIGN_SPEC (gst_ttml_text_align_spec_get_type ())
GType gst_ttml_text_align_spec_get_type (void);
#define GST_TTML_DISPLAY_ALIGN_SPEC (gst_ttml_display_align_spec_get_type ())
GType gst_ttml_display_align_spec_get_type (void);

/* Miscellaneous utility functions */
gboolean gst_ttml_utils_namespace_is_ttml (const gchar *ns);
gboolean gst_ttml_utils_attr_value_is (const gchar *str1, const gchar *str2);
GstTTMLNodeType gst_ttml_utils_node_type_parse (const gchar *ns, const gchar *name);
void gst_ttml_utils_memdump_buffer (GObject *object, const gchar *msg, GstBuffer *buffer);

/* Handy macro to dump GstBuffers to the log */
#ifndef GST_DISABLE_GST_DEBUG
#define GST_TTML_UTILS_MEMDUMP_BUFFER_OBJECT(obj,msg,buffer) \
    gst_ttml_utils_memdump_buffer ((GObject*) (obj), msg, buffer)
#define GST_TTML_UTILS_MEMDUMP_BUFFER(msg,buffer) \
    gst_ttml_utils_memdump_buffer (NULL, msg, buffer)
#else
#define GST_TTML_UTILS_MEMDUMP_BUFFER_OBJECT(object,msg,buffer)  G_STMT_START{ }G_STMT_END
#define GST_TTML_UTILS_MEMDUMP_BUFFER(msg,buffer) G_STMT_START{ }G_STMT_END
#endif

G_END_DECLS

#endif /* __GST_TTML_UTILS_H__ */
