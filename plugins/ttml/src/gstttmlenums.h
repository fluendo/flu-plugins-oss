/*
 * FLUENDO S.A.
 * Copyright (C) <2012>  <support@fluendo.com>
 */

#ifndef __GST_TTML_ENUMS_H__
#define __GST_TTML_ENUMS_H__

G_BEGIN_DECLS

/* Types of TTML nodes */
typedef enum _GstTTMLNodeType {
  GST_TTML_NODE_TYPE_UNKNOWN,
  GST_TTML_NODE_TYPE_TT,
  GST_TTML_NODE_TYPE_HEAD,
  GST_TTML_NODE_TYPE_BODY,
  GST_TTML_NODE_TYPE_DIV,
  GST_TTML_NODE_TYPE_P,
  GST_TTML_NODE_TYPE_SPAN,
  GST_TTML_NODE_TYPE_BR,
  GST_TTML_NODE_TYPE_SET,
  GST_TTML_NODE_TYPE_STYLING,
  GST_TTML_NODE_TYPE_STYLE,
  GST_TTML_NODE_TYPE_LAYOUT,
  GST_TTML_NODE_TYPE_REGION
} GstTTMLNodeType;

/* Attributes currently supported */
typedef enum _GstTTMLAttributeType {
  GST_TTML_ATTR_NODE_TYPE,
  GST_TTML_ATTR_ID,
  GST_TTML_ATTR_BEGIN,
  GST_TTML_ATTR_END,
  GST_TTML_ATTR_DUR,
  GST_TTML_ATTR_TICK_RATE,
  GST_TTML_ATTR_FRAME_RATE,
  GST_TTML_ATTR_FRAME_RATE_MULTIPLIER,
  GST_TTML_ATTR_CELLRESOLUTION,
  GST_TTML_ATTR_WHITESPACE_PRESERVE,
  GST_TTML_ATTR_SEQUENTIAL_TIME_CONTAINER,
  /* Only Styling attributes beyond this point! */
  GST_TTML_ATTR_STYLE,
  GST_TTML_ATTR_REGION,
  GST_TTML_ATTR_COLOR,
  GST_TTML_ATTR_BACKGROUND_COLOR,
  GST_TTML_ATTR_DISPLAY,
  GST_TTML_ATTR_FONT_FAMILY,
  GST_TTML_ATTR_FONT_SIZE,
  GST_TTML_ATTR_FONT_STYLE,
  GST_TTML_ATTR_FONT_WEIGHT,
  GST_TTML_ATTR_TEXT_DECORATION,
  GST_TTML_ATTR_ORIGIN,
  GST_TTML_ATTR_EXTENT,
  GST_TTML_ATTR_BACKGROUND_REGION_COLOR,
  GST_TTML_ATTR_TEXT_ALIGN,
  GST_TTML_ATTR_DISPLAY_ALIGN,
  GST_TTML_ATTR_OVERFLOW,
  GST_TTML_ATTR_TEXTOUTLINE,
  GST_TTML_ATTR_UNKNOWN
} GstTTMLAttributeType;

/* Event types */
typedef enum _GstTTMLEventType {
  GST_TTML_EVENT_TYPE_SPAN_BEGIN,
  GST_TTML_EVENT_TYPE_SPAN_END,
  GST_TTML_EVENT_TYPE_SPAN_ATTR_UPDATE,
  GST_TTML_EVENT_TYPE_REGION_BEGIN,
  GST_TTML_EVENT_TYPE_REGION_END,
  GST_TTML_EVENT_TYPE_REGION_ATTR_UPDATE
} GstTTMLEventType;

/* Length units */
typedef enum _GstTTMLLengthUnit {
  GST_TTML_LENGTH_UNIT_NOT_PRESENT = 0,
  GST_TTML_LENGTH_UNIT_PIXELS,
  GST_TTML_LENGTH_UNIT_CELLS,
  GST_TTML_LENGTH_UNIT_EM,
  GST_TTML_LENGTH_UNIT_RELATIVE,
  GST_TTML_LENGTH_UNIT_UNKNOWN
} GstTTMLLengthUnit;

/* Font Styles */
typedef enum _GstTTMLFontStyle {
  GST_TTML_FONT_STYLE_NORMAL,
  GST_TTML_FONT_STYLE_ITALIC,
  GST_TTML_FONT_STYLE_OBLIQUE,
  GST_TTML_FONT_STYLE_UNKNOWN
} GstTTMLFontStyle;

/* Font Weights */
typedef enum _GstTTMLFontWeight {
  GST_TTML_FONT_WEIGHT_NORMAL,
  GST_TTML_FONT_WEIGHT_BOLD,
  GST_TTML_FONT_WEIGHT_UNKNOWN
} GstTTMLFontWeight;

/* Text decorations (More than one can be selected) */
typedef enum _GstTTMLTextDecoration {
  GST_TTML_TEXT_DECORATION_NONE           = 0,
  GST_TTML_TEXT_DECORATION_UNDERLINE      = 1 << 0,
  GST_TTML_TEXT_DECORATION_STRIKETHROUGH  = 1 << 1,
  GST_TTML_TEXT_DECORATION_OVERLINE       = 1 << 2,
  GST_TTML_TEXT_DECORATION_UNKNOWN        = 1 << 3
} GstTTMLTextDecoration;

/* Text alignment */
typedef enum _GstTTMLTextAlign {
  GST_TTML_TEXT_ALIGN_LEFT,
  GST_TTML_TEXT_ALIGN_CENTER,
  GST_TTML_TEXT_ALIGN_RIGHT,
  GST_TTML_TEXT_ALIGN_START,
  GST_TTML_TEXT_ALIGN_END,
  GST_TTML_TEXT_ALIGN_UNKNOWN
} GstTTMLTextAlign;

/* Display alignment */
typedef enum _GstTTMLDisplayAlign {
  GST_TTML_DISPLAY_ALIGN_BEFORE,
  GST_TTML_DISPLAY_ALIGN_CENTER,
  GST_TTML_DISPLAY_ALIGN_AFTER,
  GST_TTML_DISPLAY_ALIGN_UNKNOWN
} GstTTMLDisplayAlign;

G_END_DECLS

#endif /* __GST_TTMLPRIVATE_H__ */
