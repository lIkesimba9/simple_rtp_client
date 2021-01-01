#include <iostream>
#include <gst/gst.h>

#include <gst/rtp/rtp.h>
#include <sys/time.h>
#include <queue>
#include "circular_buffer.h"
#include <thread>
#include <utility>
#include <fstream>
#include <cmath>
using namespace std;

struct timespec timeForPreFilter;
bool fForMeasTime = true;
// Константы используемые в фильтре
#define q 1e3
#define e0 0.1
#define chi {0.1, 0.001}
#define delVarTh0 12.5
#define overuseTimeTh 10
#define kU 0.01
#define kD 0.00018
#define T {0.5, 1}
#define beta 0.85
#define checkRtpTime 5
// inline-код для вычисления интервала времени в мкс
// a,b - типа timeval
// result - uint64

/**
 * @fn timespec_diff(struct timespec *, struct timespec *, struct timespec *)
 * @brief Compute the diff of two timespecs, that is a - b = result.
 * @param a the minuend
 * @param b the subtrahend
 * @param result a - b
 */
static inline int64_t
timespec_to_msec(const struct timespec *a)
{
    return (int64_t)a->tv_sec * 1000 + a->tv_nsec / 1000000;
}
/*
 * Receiver setup
 *
 *  receives H264 encoded RTP video on port 5000, RTCP is received on  port 5001.
 *  the receiver RTCP reports are sent to port 5005
 *
 *             .-------.      .----------.     .---------.   .-------.   .------------.   .-----------.
 *  RTP        |udpsrc |      | rtpbin   |     |h264depay|   |h264dec|   |videoconvert|   |xvimagesink|
 *  port=5000  |      src->recv_rtp recv_rtp->sink     src->sink   src->sink        src->sink         |
 *             '-------'      |          |     '---------'   '-------'   '------------'   '-----------'
 *                            |          |
 *                            |          |     .-------.
 *                            |          |     |udpsink|  RTCP
 *                            |    send_rtcp->sink     | port=5005
 *             .-------.      |          |     '-------' sync=false
 *  RTCP       |udpsrc |      |          |               async=false
 *  port=5001  |     src->recv_rtcp      |
 *             '-------'      '----------'
 */



static gboolean bus_call (GstBus     *bus,
                         GstMessage *msg,
                         gpointer    data)
{
    GMainLoop *loop = (GMainLoop *)data;

    switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
        g_print ("End-of-stream\n");
        g_main_loop_quit (loop);
        break;
    case GST_MESSAGE_ERROR: {
        gchar *debug = NULL;
        GError *err = NULL;

        gst_message_parse_error (msg, &err, &debug);

        g_print ("Error: %s\n", err->message);
        g_error_free (err);

        if (debug) {
            g_print ("Debug details: %s\n", debug);
            g_free (debug);
        }


        g_main_loop_quit (loop);
        break;
    }
    case GST_MESSAGE_INFO: {
        gchar *debug = NULL;
        GError *err = NULL;

        gst_message_parse_info(msg, &err, &debug);

        g_print ("INFO: %s\n", err->message);
        g_error_free (err);

        if (debug) {
            g_print ("Debug details: %s\n", debug);
            g_free (debug);
        }


        break;
    }
    case GST_MESSAGE_WARNING: {
        gchar *debug = NULL;
        GError *err = NULL;

        gst_message_parse_warning(msg, &err, &debug);

        g_print ("WARNING: %s\n", err->message);
        g_error_free (err);

        if (debug) {
            g_print ("Debug details: %s\n", debug);
            g_free (debug);
        }


        break;
    }



    default:
        break;
    }

    return TRUE;
}



static void cb_new_rtp_recv_src_pad (GstElement *element,
                                    GstPad     *pad,
                                    gpointer    data)
{

    GstElement *rtpx264depay = (GstElement *) data;
    GstPad *sinkPad;
    sinkPad = gst_element_get_static_pad (rtpx264depay, "sink");
    gst_pad_link (pad, sinkPad);
    gst_object_unref (sinkPad);

}

bool linkStaticAndRequestPads(GstElement *sourse,GstElement *sink,gchar *nameSrcPad,gchar *nameSinkPad)
{

    GstPad *srcPad = gst_element_get_static_pad(sourse,nameSrcPad);
    GstPad *sinkPad = gst_element_get_request_pad(sink,nameSinkPad);
    GstPadLinkReturn ret_link = gst_pad_link(srcPad,sinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, static and request pad\n";
        return false;
    }
    gst_object_unref(GST_OBJECT(srcPad));
    gst_object_unref(GST_OBJECT(sinkPad));
    return true;
}
bool linkRequestAndStaticPads(GstElement *sourse,GstElement *sink,gchar *nameSrcPad,gchar *nameSinkPad)
{

    GstPad *srcPad = gst_element_get_request_pad(sourse,nameSrcPad);
    GstPad *sinkPad = gst_element_get_static_pad(sink,nameSinkPad);
    GstPadLinkReturn ret_link = gst_pad_link(srcPad,sinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, request and statitc pad\n";
        return false;
    }
    gst_object_unref(GST_OBJECT(srcPad));
    gst_object_unref(GST_OBJECT(sinkPad));
    return true;
}

// для отладки
ofstream outBeforePreFilter;
ofstream afterPrefilter;

GstPadProbeReturn cb_read_time_from_rtp_pakcet (GstPad *pad,
                                               GstPadProbeInfo *info,gpointer data)
{


    //circular_buffer<int64_t> *rtpTimeBuffer = (circular_buffer<int64_t> *)data;
    GstBuffer *buffer;
    buffer = GST_PAD_PROBE_INFO_BUFFER (info);
    if (buffer == NULL)
        return GST_PAD_PROBE_OK;

    static int64_t oldRecvTime;
    static int64_t oldSendTime;
    gpointer miliSec;
    guint size = 3;
    GstRTPBuffer rtpBufer = GST_RTP_BUFFER_INIT;
    double fMax = 0;
    double tmpFMax;
    gst_rtp_buffer_map(buffer,GST_MAP_READ,&rtpBufer);
    // Получаю метку, которые засунул на сервере.
    gst_rtp_buffer_get_extension_onebyte_header(&rtpBufer,1,0,&miliSec,&size);
    if (miliSec != 0){
        struct timespec mtRecv;
        clock_gettime (CLOCK_REALTIME, &mtRecv);
        mtRecv.tv_sec = mtRecv.tv_sec & 63;
        mtRecv.tv_nsec = mtRecv.tv_nsec & 0xffffffffffffc000;

        //unsigned int nsec = ( unsigned int )mt.tv_nsec;
        //unsigned int sec = ( unsigned int )mt.tv_sec;
        //int recvTime = ( (nsec >> 14) | (sec << 18) ) & 0x00ffffff;
        unsigned int sendTime = *((long long *)miliSec) & 0x00ffffff; //Milisec
        struct timespec mtSend;
        mtSend.tv_sec = sendTime >> 18;
        mtSend.tv_nsec = sendTime << 14;
        // PRE FILTER CODE !!!
        if(fForMeasTime)
        {
            outBeforePreFilter.open("out_before_pre_filtet.txt");
            afterPrefilter.open("out_after_pre_filtet.txt");

            clock_gettime(CLOCK_REALTIME,&timeForPreFilter);
            fForMeasTime = false;
            oldRecvTime = timespec_to_msec(&mtRecv);
            oldSendTime = timespec_to_msec(&mtSend);

        }
        else {
            int64_t diffRecv = timespec_to_msec(&mtRecv) - oldRecvTime;
            int64_t diffSend = timespec_to_msec(&mtSend) - oldSendTime;
            oldRecvTime = timespec_to_msec(&mtRecv);
            oldSendTime = timespec_to_msec(&mtSend);
            int64_t diff = diffRecv - diffSend;
            if(diffSend != 0)
                tmpFMax = 1 / diffSend;
            else
                tmpFMax = 0;
            fMax =  tmpFMax > fMax ? tmpFMax : fMax;

            struct timespec curTime;
            clock_gettime(CLOCK_REALTIME,&curTime);
            if ( ( (timespec_to_msec(&curTime) - timespec_to_msec(&timeForPreFilter) ) > checkRtpTime ) || diff < 0 )
            {
              //  rtpTimeBuffer->put(diff);
              afterPrefilter << "diff: " << diff << " " << fMax << '\n';
                clock_gettime(CLOCK_REALTIME,&timeForPreFilter);
            }
        }
        // КОнец пре фильтера.
    }

    //cerr << "RECV time, milisec: " << *((long long *)miliSec) << '\n';


    gst_rtp_buffer_unmap(&rtpBufer);
    return GST_PAD_PROBE_OK;
}



GstElement *create_pipeline(circular_buffer<int64_t> &buffer){

    //queue<long long> rtpRecvTime;
    GstElement *pipeline,*udpSrcRtp,*videconverter,
        *x264decoder,*rtph264depay,*xvimagesink,
        *rtpbin,*udpSrcRtcp,*udpSinkRtcp;

    pipeline = gst_pipeline_new("rtpStreamerRecv");

    // Создаю udpsrc для приема rtp пакетов.
    udpSrcRtp = gst_element_factory_make("udpsrc","source");
    // Создаю элемент управющий rtp сесией
    rtpbin = gst_element_factory_make("rtpbin","rtpbin");

    // Создаю udp сток для приема rtсp пакетов
    udpSinkRtcp = gst_element_factory_make("udpsink","udpSinkRtcp");
    // Создаю udp источник для отправки rtcp пакетов.
    udpSrcRtcp = gst_element_factory_make("udpsrc","udpSrcRtcp");

    // Создаю элемент который распакуют данные из rtp пакетов.
    rtph264depay = gst_element_factory_make("rtph264depay","rtpdepay");
    // Создаю декодер
    x264decoder = gst_element_factory_make("avdec_h264","decoder");



    // Создаю элелмент который преобразует данные с кодера для воспроизведения.
    videconverter = gst_element_factory_make("videoconvert","converter");
    // Создаю для показа видео входящего видео потока.
    xvimagesink = gst_element_factory_make("xvimagesink","video");


    if (!pipeline || !udpSrcRtp || !x264decoder || !rtph264depay || !rtpbin || !udpSrcRtp || !udpSinkRtcp || !udpSrcRtcp || !xvimagesink || !videconverter)
    {
        cerr << "Not all elements could be created.\n";
        return NULL;
    }
    // Задаю свойство udpsrt для приема RTP пакетов с которог захватывать видео
    g_object_set(G_OBJECT(udpSrcRtp),"caps",gst_caps_from_string("application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=(string)H264"),NULL);
    g_object_set(G_OBJECT(udpSrcRtp),"port",5000,NULL);


    // Устанавливаю параметры для upd сойденений.
    g_object_set(G_OBJECT(udpSrcRtcp),"address","127.0.0.1",NULL);
    g_object_set(G_OBJECT(udpSrcRtcp),"port",5001,NULL);
    g_object_set(G_OBJECT (udpSrcRtcp), "caps", gst_caps_from_string("application/x-rtcp"), NULL);


    g_object_set(G_OBJECT(udpSinkRtcp),"host","127.0.0.1",NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"port",5005,NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"sync",FALSE,NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"async",FALSE,NULL);


    g_object_set(G_OBJECT (rtpbin), "latency", 500, NULL);
    // Добавляю элементы в контейнер
    gst_bin_add_many(GST_BIN(pipeline),udpSrcRtp,udpSrcRtcp,rtpbin,udpSinkRtcp,
                     rtph264depay,x264decoder,videconverter,xvimagesink,NULL);

    // Сойденяю PADы.

    if (!linkStaticAndRequestPads(udpSrcRtp,rtpbin,"src","recv_rtp_sink_%u"))
    {
        cerr << "Error create link, beetwen udpSrcRtp and rtpbin\n";
        return NULL;
    }



    if (!linkStaticAndRequestPads(udpSrcRtcp,rtpbin,"src","recv_rtcp_sink_%u"))
    {
        cerr << "Error create link, beetwen udpSrcRtcp and rtpbin\n";
        return NULL;
    }



    if (!linkRequestAndStaticPads(rtpbin,udpSinkRtcp,"send_rtcp_src_%u","sink"))
    {
        cerr << "Error create link, beetwen rtpbin and udpSinkRtcp\n";
        return NULL;
    }

    // сойденяю остальные элементы
    // Подключаю сигнал для ПАДа, который доступен иногда.
    g_signal_connect (rtpbin, "pad-added", G_CALLBACK (cb_new_rtp_recv_src_pad),rtph264depay);


    if (!gst_element_link_many(rtph264depay,x264decoder,videconverter,xvimagesink,NULL))
    {
        cerr << "Elements could not be linked other.\n";
        return NULL;

    }



    // Подключаю обработку ПЭДа, для получение временной метки.
    GstPad *rtph264depayPad = gst_element_get_static_pad(rtph264depay,"sink");
    gst_pad_add_probe(rtph264depayPad,GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)cb_read_time_from_rtp_pakcet,&buffer,NULL);
    gst_object_unref(GST_OBJECT(rtph264depayPad));

    return pipeline;

}
void kalmanFilter(circular_buffer<int64_t> &buffer)
{
   // double alpha = pow((1 - chi),(30 / ( 1000 *
    //f_max = max {1/(T(j) - T(j-1))}
    //double K_i = (e0 + q) /

    while (true) {
        if (!buffer.empty())
        {
          //  cerr << "Buffer size is: " << buffer.size() << '\n';

          int d_i = buffer.get();
          cerr << "Time after prefiler: " << d_i << '\n';

        }
    }
}
int main(int argc, char *argv[])
{

    gst_init(&argc, &argv);
    //  GST_LEVEL_DEBUG;
    circular_buffer<int64_t> circle(10);
    //  circle.put(1);
   // thread callbackMechanismTh(kalmanFilter,ref(circle));

    //callbackMechanismTh.detach();
    GMainLoop *loop;
    loop = g_main_loop_new(NULL,FALSE);
    GstBus *bus;

    GstElement *pipeline = create_pipeline(circle);
    if (pipeline == NULL)
    {
        cerr << "Error create pipeline!" << endl;
        return -1;
    }

    guint watch_id;
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);


    GstStateChangeReturn ret;
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    g_main_loop_run (loop);



    /* clean up */

    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    g_source_remove (watch_id);
    g_main_loop_unref (loop);
    outBeforePreFilter.close();
    afterPrefilter.close();

    return 0;
}
