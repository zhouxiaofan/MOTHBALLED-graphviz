/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/**********************************************************
*      This software is part of the graphviz package      *
*                http://www.graphviz.org/                 *
*                                                         *
*            Copyright (c) 1994-2004 AT&T Corp.           *
*                and is licensed under the                *
*            Common Public License, Version 1.0           *
*                      by AT&T Corp.                      *
*                                                         *
*        Information and Software Systems Research        *
*              AT&T Research, Florham Park NJ             *
**********************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "const.h"

#include "gvplugin_render.h"
#include "gvplugin_device.h"
#include "gvio.h"
#include "graph.h"

typedef enum { FORMAT_VML, FORMAT_VMLZ, } format_type;

extern char *xml_string(char *str);

char graphcoords[256];

#if defined(WIN32) && !defined(__MINGW32_VERSION)	/* MinGW already defines snprintf */
static int
snprintf (char *str, int n, char *fmt, ...)
{
int ret;
va_list a;
va_start (a, fmt);
ret = _vsnprintf (str, n, fmt, a);
va_end (a);
return ret;
}
#endif

static void vml_bzptarray(GVJ_t * job, pointf * A, int n)
{
    int i;
    char *c;

    c = "m ";			/* first point */
    for (i = 0; i < n; i++) {
	gvprintf(job, "%s%.0f,%.0f ", c, A[i].x, -A[i].y);
	if (i == 0)
	    c = "c ";		/* second point */
	else
	    c = "";		/* remaining points */
    }
}

static void vml_print_color(GVJ_t * job, gvcolor_t color)
{
    switch (color.type) {
    case COLOR_STRING:
	gvputs(job, color.u.string);
	break;
    case RGBA_BYTE:
	if (color.u.rgba[3] == 0) /* transparent */
	    gvputs(job, "none");
	else
	    gvprintf(job, "#%02x%02x%02x",
		color.u.rgba[0], color.u.rgba[1], color.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
}

static void vml_grstroke(GVJ_t * job, int filled)
{
    obj_state_t *obj = job->obj;

    gvputs(job, "<v:stroke fillcolor=\"");
    if (filled)
	vml_print_color(job, obj->fillcolor);
    else
	gvputs(job, "none");
    gvputs(job, "\" strokecolor=\"");
    vml_print_color(job, obj->pencolor);
    if (obj->penwidth != PENWIDTH_NORMAL)
	gvprintf(job, "\" stroke-weight=\"%g", obj->penwidth);
    if (obj->pen == PEN_DASHED) {
	gvputs(job, "\" dashstyle=\"dash");
    } else if (obj->pen == PEN_DOTTED) {
	gvputs(job, "\" dashstyle=\"dot");
    }
    gvputs(job, "\" />");
}

static void vml_grstrokeattr(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    gvputs(job, " strokecolor=\"");
    vml_print_color(job, obj->pencolor);
    if (obj->penwidth != PENWIDTH_NORMAL)
	gvprintf(job, "\" stroke-weight=\"%g", obj->penwidth);
    if (obj->pen == PEN_DASHED) {
	gvputs(job, "\" dashstyle=\"dash");
    } else if (obj->pen == PEN_DOTTED) {
	gvputs(job, "\" dashstyle=\"dot");
    }
    gvputs(job, "\"");
}

static void vml_grfill(GVJ_t * job, int filled)
{
    obj_state_t *obj = job->obj;

    gvputs(job, "<v:fill color=\"");
    if (filled)
	vml_print_color(job, obj->fillcolor);
    else
	gvputs(job, "none");
    gvputs(job, "\" />");
}

static void vml_comment(GVJ_t * job, char *str)
{
    gvputs(job, "      <!-- ");
    gvputs(job, xml_string(str));
    gvputs(job, " -->\n");
}

static void vml_begin_job(GVJ_t * job)
{
    gvputs(job, "<?xml version=\"1.1\" encoding=\"UTF-8\" ?>\n");

    gvputs(job, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" ");
    gvputs(job, "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n");
    gvputs(job, "<html xml:lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\" ");
    gvputs(job, "xmlns:v=\"urn:schemas-microsoft-com:vml\""); 
    gvputs(job, ">"); 

    gvputs(job, "\n<!-- Generated by ");
    gvputs(job, xml_string(job->common->info[0]));
    gvputs(job, " version ");
    gvputs(job, xml_string(job->common->info[1]));
    gvputs(job, " (");
    gvputs(job, xml_string(job->common->info[2]));
    gvputs(job, ")\n     For user: ");
    gvputs(job, xml_string(job->common->user));
    gvputs(job, " -->\n");
}

static void vml_begin_graph(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    gvputs(job, "<head>");
    if (obj->u.g->name[0]) {
        gvputs(job, "<title>");
	gvputs(job, xml_string(obj->u.g->name));
        gvputs(job, "</title>");
    }
    gvprintf(job, "<!-- Pages: %d -->\n</head>\n", job->pagesArraySize.x * job->pagesArraySize.y);

    snprintf(graphcoords, sizeof(graphcoords), "style=\"width: %.0fpt; height: %.0fpt\" coordsize=\"%.0f,%.0f\" coordorigin=\"-4,-%.0f\"",
	job->width*.75, job->height*.75,
        job->width*.75, job->height*.75,
        job->height*.75 - 4);

    gvprintf(job, "<body>\n<div class=\"graph\" %s>\n", graphcoords);
    gvputs(job, "<style type=\"text/css\">\nv\\:* {\nbehavior: url(#default#VML);display:inline-block;position: absolute; left: 0px; top: 0px;\n}\n</style>\n");
/*    graphcoords[0] = '\0'; */

}

static void vml_end_graph(GVJ_t * job)
{
    gvputs(job, "</div>\n</body>\n");
}

static void
vml_begin_anchor(GVJ_t * job, char *href, char *tooltip, char *target, char *id)
{
    gvputs(job, "      <a");
    if (href && href[0])
	gvprintf(job, " href=\"%s\"", xml_string(href));
    if (tooltip && tooltip[0])
	gvprintf(job, " title=\"%s\"", xml_string(tooltip));
    if (target && target[0])
	gvprintf(job, " target=\"%s\"", xml_string(target));
    gvputs(job, ">\n");
}

static void vml_end_anchor(GVJ_t * job)
{
    gvputs(job, "      </a>\n");
}

static void vml_textpara(GVJ_t * job, pointf p, textpara_t * para)
{
    obj_state_t *obj = job->obj;

    gvputs(job, "        <div");
    switch (para->just) {
    case 'l':
	gvputs(job, " style=\"text-align: left; ");
	break;
    case 'r':
	gvputs(job, " style=\"text-align: right; ");
	break;
    default:
    case 'n':
	gvputs(job, " style=\"text-align: center; ");
	break;
    }
    gvprintf(job, "position: absolute; left: %gpx; top: %gpx;", p.x/.75, job->height - p.y/.75 - 14);
    if (para->postscript_alias) {
        gvprintf(job, " font-family: '%s';", para->postscript_alias->family);
        if (para->postscript_alias->weight)
	    gvprintf(job, " font-weight: %s;", para->postscript_alias->weight);
        if (para->postscript_alias->stretch)
	    gvprintf(job, " font-stretch: %s;", para->postscript_alias->stretch);
        if (para->postscript_alias->style)
	    gvprintf(job, " font-style: %s;", para->postscript_alias->style);
    }
    else {
        gvprintf(job, " font-family: \'%s\';", para->fontname);
    }
    /* FIXME - even inkscape requires a magic correction to fontsize.  Why?  */
    gvprintf(job, " font-size: %.2fpt;", para->fontsize * 0.81);
    switch (obj->pencolor.type) {
    case COLOR_STRING:
	if (strcasecmp(obj->pencolor.u.string, "black"))
	    gvprintf(job, "color:%s;", obj->pencolor.u.string);
	break;
    case RGBA_BYTE:
	gvprintf(job, "color:#%02x%02x%02x;",
		obj->pencolor.u.rgba[0], obj->pencolor.u.rgba[1], obj->pencolor.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
    gvputs(job, "\">");
    gvputs(job, xml_string(para->str));
    gvputs(job, "</div>\n");
}

static void vml_ellipse(GVJ_t * job, pointf * A, int filled)
{
    /* A[] contains 2 points: the center and corner. */
    
    gvputs(job, "        <v:oval");

    vml_grstrokeattr(job);

    gvputs(job, " style=\"position: absolute;");

    gvprintf(job, " left:  %gpt; top:    %gpt;", 2*A[0].x - A[1].x+4, job->height*.75 - A[1].y-4);
    gvprintf(job, " width: %gpt; height: %gpt;", 2*(A[1].x - A[0].x), 2*(A[1].y - A[0].y));
    gvputs(job, "\">");
    vml_grstroke(job, filled);
    vml_grfill(job, filled);
    gvputs(job, "</v:oval>\n");
}

static void
vml_bezier(GVJ_t * job, pointf * A, int n, int arrow_at_start,
	      int arrow_at_end, int filled)
{
    gvprintf(job, "        <v:shape %s><!-- bezier --><v:path", graphcoords);
    gvputs(job, " v=\"");
    vml_bzptarray(job, A, n);
    gvputs(job, "\" />");
    vml_grstroke(job, filled);
    gvputs(job, "</v:path>");
    vml_grfill(job, filled);
    gvputs(job, "</v:shape>\n");
}

static void vml_polygon(GVJ_t * job, pointf * A, int n, int filled)
{
    int i;

    gvputs(job, "        <v:shape");
    vml_grstrokeattr(job);
    gvprintf(job, " %s><!-- polygon --><v:path", graphcoords);
    gvputs(job, " v=\"");
    for (i = 0; i < n; i++)
    {
        if (i==0) gvputs(job, "m ");
	gvprintf(job, "%.0f,%.0f ", A[i].x, -A[i].y);
        if (i==0) gvputs(job, "l ");
        if (i==n-1) gvputs(job, "x e ");
    }
    gvputs(job, "\">");
    vml_grstroke(job, filled);
    gvputs(job, "</v:path>");
    vml_grfill(job, filled);
    gvputs(job, "</v:shape>\n");
}

static void vml_polyline(GVJ_t * job, pointf * A, int n)
{
    int i;

    gvprintf(job, "        <v:shape %s><!-- polyline --><v:path", graphcoords);
    gvputs(job, " v=\"");
    for (i = 0; i < n; i++)
    {
        if (i==0) gvputs(job, " m ");
	gvprintf(job, "%.0f,%.0f ", A[i].x, -A[i].y);
        if (i==0) gvputs(job, " l ");
        if (i==n-1) gvputs(job, " e "); /* no x here for polyline */
    }
    gvputs(job, "\">");
    vml_grstroke(job, 0);                 /* no fill here for polyline */
    gvputs(job, "</v:path>");
    gvputs(job, "</v:shape>\n");

}

/* color names from http://www.w3.org/TR/VML/types.html */
/* NB.  List must be LANG_C sorted */
static char *vml_knowncolors[] = {
    "aliceblue", "antiquewhite", "aqua", "aquamarine", "azure",
    "beige", "bisque", "black", "blanchedalmond", "blue",
    "blueviolet", "brown", "burlywood",
    "cadetblue", "chartreuse", "chocolate", "coral",
    "cornflowerblue", "cornsilk", "crimson", "cyan",
    "darkblue", "darkcyan", "darkgoldenrod", "darkgray",
    "darkgreen", "darkgrey", "darkkhaki", "darkmagenta",
    "darkolivegreen", "darkorange", "darkorchid", "darkred",
    "darksalmon", "darkseagreen", "darkslateblue", "darkslategray",
    "darkslategrey", "darkturquoise", "darkviolet", "deeppink",
    "deepskyblue", "dimgray", "dimgrey", "dodgerblue",
    "firebrick", "floralwhite", "forestgreen", "fuchsia",
    "gainsboro", "ghostwhite", "gold", "goldenrod", "gray",
    "green", "greenyellow", "grey",
    "honeydew", "hotpink", "indianred",
    "indigo", "ivory", "khaki",
    "lavender", "lavenderblush", "lawngreen", "lemonchiffon",
    "lightblue", "lightcoral", "lightcyan", "lightgoldenrodyellow",
    "lightgray", "lightgreen", "lightgrey", "lightpink",
    "lightsalmon", "lightseagreen", "lightskyblue",
    "lightslategray", "lightslategrey", "lightsteelblue",
    "lightyellow", "lime", "limegreen", "linen",
    "magenta", "maroon", "mediumaquamarine", "mediumblue",
    "mediumorchid", "mediumpurple", "mediumseagreen",
    "mediumslateblue", "mediumspringgreen", "mediumturquoise",
    "mediumvioletred", "midnightblue", "mintcream",
    "mistyrose", "moccasin",
    "navajowhite", "navy", "oldlace",
    "olive", "olivedrab", "orange", "orangered", "orchid",
    "palegoldenrod", "palegreen", "paleturquoise",
    "palevioletred", "papayawhip", "peachpuff", "peru", "pink",
    "plum", "powderblue", "purple",
    "red", "rosybrown", "royalblue",
    "saddlebrown", "salmon", "sandybrown", "seagreen", "seashell",
    "sienna", "silver", "skyblue", "slateblue", "slategray",
    "slategrey", "snow", "springgreen", "steelblue",
    "tan", "teal", "thistle", "tomato", "turquoise",
    "violet",
    "wheat", "white", "whitesmoke",
    "yellow", "yellowgreen"
};

gvrender_engine_t vml_engine = {
    vml_begin_job,
    0,				/* vml_end_job */
    vml_begin_graph,
    vml_end_graph,
    0,                          /* vml_begin_layer */
    0,                          /* vml_end_layer */
    0,                          /* vml_begin_page */
    0,                          /* vml_end_page */
    0,                          /* vml_begin_cluster */
    0,                          /* vml_end_cluster */
    0,				/* vml_begin_nodes */
    0,				/* vml_end_nodes */
    0,				/* vml_begin_edges */
    0,				/* vml_end_edges */
    0,                          /* vml_begin_node */
    0,                          /* vml_end_node */
    0,                          /* vml_begin_edge */
    0,                          /* vml_end_edge */
    vml_begin_anchor,
    vml_end_anchor,
    vml_textpara,
    0,				/* vml_resolve_color */
    vml_ellipse,
    vml_polygon,
    vml_bezier,
    vml_polyline,
    vml_comment,
    0,				/* vml_library_shape */
};

gvrender_features_t render_features_vml = {
    GVRENDER_Y_GOES_DOWN
        | GVRENDER_DOES_TRANSFORM
	| GVRENDER_DOES_LABELS
	| GVRENDER_DOES_MAPS
	| GVRENDER_DOES_TARGETS
	| GVRENDER_DOES_TOOLTIPS, /* flags */
    4.,                         /* default pad - graph units */
    vml_knowncolors,		/* knowncolors */
    sizeof(vml_knowncolors) / sizeof(char *),	/* sizeof knowncolors */
    RGBA_BYTE,			/* color_type */
};

gvdevice_features_t device_features_vml = {
    GVDEVICE_DOES_TRUECOLOR,	/* flags */
    {0.,0.},			/* default margin - points */
    {0.,0.},                    /* default page width, height - points */
    {96.,96.},			/* default dpi */
};

gvdevice_features_t device_features_vmlz = {
    GVDEVICE_DOES_TRUECOLOR
      | GVDEVICE_COMPRESSED_FORMAT,	/* flags */
    {0.,0.},			/* default margin - points */
    {0.,0.},                    /* default page width, height - points */
    {96.,96.},			/* default dpi */
};

gvplugin_installed_t gvrender_vml_types[] = {
    {FORMAT_VML, "vml", 1, &vml_engine, &render_features_vml},
    {0, NULL, 0, NULL, NULL}
};

gvplugin_installed_t gvdevice_vml_types[] = {
    {FORMAT_VML, "vml:vml", 1, NULL, &device_features_vml},
#if HAVE_LIBZ
    {FORMAT_VMLZ, "vmlz:vml", 1, NULL, &device_features_vmlz},
#endif
    {0, NULL, 0, NULL, NULL}
};
