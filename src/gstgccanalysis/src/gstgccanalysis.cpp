/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2021 simba9 <<user@hostname.org>>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-gccanalysis
 *
 * FIXME:Describe gccanalysis here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! gccanalysis ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif



#include <gst/gst.h>
#include <gst/rtp/rtp.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include "gstgccanalysis.h"
#include "log.cpp"

using namespace std;

// Константы используемые в фильтре
#define q 1e3
#define e0 0.1
//#define chi {0.1, 0.001}
#define chi 0.0505
#define delVarTh0 12.5
#define overuseTimeTh 10
#define kU 0.01
#define kD 0.00018
#define T {0.5, 1}
#define beta 0.85
#define checkRtpTime 5



GST_DEBUG_CATEGORY_STATIC (gst_gcc_analysis_debug);
#define GST_CAT_DEFAULT gst_gcc_analysis_debug

/* Filter signals and args */
enum
{
    /* FILL ME */
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
                                                                   GST_PAD_SINK,
                                                                   GST_PAD_ALWAYS,
                                                                   GST_STATIC_CAPS ("ANY")
                                                                   );


#define gst_gcc_analysis_parent_class parent_class
G_DEFINE_TYPE (GstGccAnalysis, gst_gcc_analysis, GST_TYPE_ELEMENT);


static void gst_gcc_analysis_set_property (GObject * object, guint prop_id,
                                          const GValue * value, GParamSpec * pspec);
static void gst_gcc_analysis_get_property (GObject * object, guint prop_id,
                                          GValue * value, GParamSpec * pspec);

static gboolean gst_gcc_analysis_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_gcc_analysis_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);


// Прототипы моих функций

static inline int64_t timespec_to_msec(const struct timespec *a);

inline void write_header_in_csv_file(vector<string> headers,ofstream &out);
inline void write_data_in_csv_file(vector<int64_t> values,ofstream &out);

/* GObject vmethod implementations */

/* initialize the gccanalysis's class */
static void gst_gcc_analysis_class_init (GstGccAnalysisClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;

    gobject_class->set_property = gst_gcc_analysis_set_property;
    gobject_class->get_property = gst_gcc_analysis_get_property;

    g_object_class_install_property (gobject_class, PROP_SILENT,
                                    g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
                                                         FALSE, G_PARAM_READWRITE));

    gst_element_class_set_details_simple(gstelement_class,
                                         "GccAnalysis",
                                         "For analysis time delay.",
                                         "PGSD",
                                         "simba9 ostilia@mail.ru");


    gst_element_class_add_pad_template (gstelement_class,
                                       gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void gst_gcc_analysis_init (GstGccAnalysis * gcc)
{
    gcc->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
    gst_pad_set_event_function (gcc->sinkpad,
                               GST_DEBUG_FUNCPTR(gst_gcc_analysis_sink_event));
    gst_pad_set_chain_function (gcc->sinkpad,
                               GST_DEBUG_FUNCPTR(gst_gcc_analysis_chain));
    GST_PAD_SET_PROXY_CAPS (gcc->sinkpad);
    gst_element_add_pad (GST_ELEMENT (gcc), gcc->sinkpad);

    gcc->l.setFileName("../samples/datanew.csv");
    gcc->l.setHeader({"diff","diffGroup"});

    gcc->silent = FALSE;
    gcc->fFirstPacketGroup = true;
    gcc->fForMeasTime = true;

}

static void gst_gcc_analysis_set_property (GObject * object, guint prop_id,
                                          const GValue * value, GParamSpec * pspec)
{
    GstGccAnalysis *gcc = GST_GCCANALYSIS (object);

    switch (prop_id) {
    case PROP_SILENT:
        gcc->silent = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void gst_gcc_analysis_get_property (GObject * object, guint prop_id,
                                          GValue * value, GParamSpec * pspec)
{
    GstGccAnalysis *gcc = GST_GCCANALYSIS (object);

    switch (prop_id) {
    case PROP_SILENT:
        g_value_set_boolean (value, gcc->silent);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean gst_gcc_analysis_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
    GstGccAnalysis *gcc;
    gboolean ret;

    gcc = GST_GCCANALYSIS (parent);

    GST_LOG_OBJECT (gcc, "Received %s event: %" GST_PTR_FORMAT,
                   GST_EVENT_TYPE_NAME (event), event);

    switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
        GstCaps * caps;

        gst_event_parse_caps (event, &caps);
        /* do something with the caps */

        /* and forward */
        ret = gst_pad_event_default (pad, parent, event);
        break;
    }
    default:
        ret = gst_pad_event_default (pad, parent, event);
        break;
    }
    return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn gst_gcc_analysis_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
    GstGccAnalysis *gcc;
    gcc = GST_GCCANALYSIS (parent);



    GstRTPBuffer rtpBufer = GST_RTP_BUFFER_INIT;
    gst_rtp_buffer_map(buf,GST_MAP_READ,&rtpBufer);
    // Получаю метку, которые засунул на сервере.
    gpointer miliSec;
    guint size = 3;
    gst_rtp_buffer_get_extension_onebyte_header(&rtpBufer,1,0,&miliSec,&size);

    //unsigned int test = *((long long *)miliSec) & 0x00ffffff;

    // g_print("time %d",test);
    double tmpFMax;

    // Получаю метку, которые засунул на сервере.


    if (miliSec != 0){

        struct timespec mtRecv;
        clock_gettime (CLOCK_REALTIME, &mtRecv);
        mtRecv.tv_sec = mtRecv.tv_sec & 63;
        mtRecv.tv_nsec = mtRecv.tv_nsec & 0xffffffffffffc000;


        unsigned int sendTime = *((long long *)miliSec) & 0x00ffffff; //Milisec
        struct timespec mtSend;
        mtSend.tv_sec = sendTime >> 18;
        mtSend.tv_nsec = sendTime << 14;


        // PRE FILTER CODE !!!
        if(gcc->fForMeasTime)
        {
            // Для отладки октрытие файлов
            // outLogData.open("data12.csv");
            // outLogData  <<"diff,Group size" << '\n';



            clock_gettime(CLOCK_REALTIME,&gcc->timeForPreFilter);
            gcc->fForMeasTime = false;
            gcc->oldRecvTime = timespec_to_msec(&mtRecv);
            gcc->oldSendTime = timespec_to_msec(&mtSend);

        }
        else {


            int64_t diffRecv = timespec_to_msec(&mtRecv) - gcc->oldRecvTime;
            int64_t diffSend = timespec_to_msec(&mtSend) - gcc->oldSendTime;
            gcc->oldRecvTime = timespec_to_msec(&mtRecv);
            gcc->oldSendTime = timespec_to_msec(&mtSend);
            int64_t diff = diffRecv - diffSend;

            struct timespec curTime;
            clock_gettime(CLOCK_REALTIME,&curTime);
            gcc->packetsInGroups++;
            if ( diff < 0 || ( timespec_to_msec(&curTime) - timespec_to_msec(&gcc->timeForPreFilter) ) > checkRtpTime )
            {
                if (gcc->fFirstPacketGroup)
                {

                    gcc->oldRecvGroupTime = gcc->oldRecvTime;
                    clock_gettime(CLOCK_REALTIME,&gcc->timeForPreFilter);
                    gcc->fFirstPacketGroup = false;

                }
                else {
                    //cerr << diff << '\n';
                    //g_print("diff %ld\n",diff);
                    if((gcc->oldRecvTime - gcc->oldRecvGroupTime) != 0)
                        tmpFMax = 1 / (gcc->oldRecvTime - gcc->oldRecvGroupTime);
                    else
                        tmpFMax = 0;

                    gcc->fMax =  tmpFMax > gcc->fMax ? tmpFMax : gcc->fMax;
                    gcc->oldRecvGroupTime = gcc->oldRecvTime;




                    //                    outLogData  << diff << "," << packetsInGroups<< '\n';
                    gcc->packetsInGroups = 0;
                    //double alpha = pow((1 - chi),30 / (1000 * fMax));

                    clock_gettime(CLOCK_REALTIME,&gcc->timeForPreFilter);
                }

            }
        }
        // КОнец пре фильтера.
    }


    return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean gccanalysis_init (GstPlugin * gccanalysis)
{
    /* debug category for fltering log messages
   *
   * exchange the string 'Template gccanalysis' with your description
   */
    GST_DEBUG_CATEGORY_INIT (gst_gcc_analysis_debug, "gccanalysis",
                            0, "Template gccanalysis");

    return gst_element_register (gccanalysis, "gccanalysis", GST_RANK_NONE,
                                GST_TYPE_GCCANALYSIS);
}

/* PACKAGE: this is usually set by meson depending on some _INIT macro
 * in meson.build and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use meson to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstgccanalysis"
#endif

/* gstreamer looks for this structure to register gccanalysiss
 *
 * exchange the string 'Template gccanalysis' with your gccanalysis description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gccanalysis,
    "Template gccanalysis",
    gccanalysis_init,
    "0.1",
    "LGPL",
    "GStreamer GCC Plugin",
    "ostilia@mail.ru"
    )

static inline int64_t timespec_to_msec(const struct timespec *a)
{
    return (int64_t)a->tv_sec * 1000 + a->tv_nsec / 1000000;
}

inline void write_header_in_csv_file(vector<string> headers,ofstream &out)
{

    for (int i = 0; i < headers.size() - 1; i++)
        out  << headers[i] << ",";

    out << headers[headers.size() - 1] << '\n';
}
inline void write_data_in_csv_file(vector<int64_t> values,ofstream &out)
{
    for (int i = 0; i < values.size() - 1; i++)
        out  << values[i] << ",";

    out << values[values.size() - 1] << '\n';


}
