#include <iostream>
#include <gst/gst.h>

#include <gst/rtp/rtp.h>
using namespace std;

/*
 * Receiver setup
 *
 *  receives H264 encoded RTP video on port 5000, RTCP is received on  port 5001.
 *  the receiver RTCP reports are sent to port 5005
 *
 *             .-------.      .----------.     .---------.   .-------.   .-----------.
 *  RTP        |udpsrc |      | rtpbin   |     |h264depay|   |h264dec|   |xvimagesink|
 *  port=5000  |      src->recv_rtp recv_rtp->sink     src->sink   src->sink         |
 *             '-------'      |          |     '---------'   '-------'   '-----------'
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
    default:
        break;
    }

    return TRUE;
}



static void cb_new_rtp_recv_src_pad (GstElement *element,
                        GstPad     *pad,
                        gpointer    data)
{
    //gchar *name;
    GstElement *rtpdec = (GstElement *) data;
    GstPad *sinkpad;

    //name = gst_pad_get_name (pad);
    //g_debug ("A new pad %s was created\n", name);
    //g_free (name);

    sinkpad = gst_element_get_static_pad (rtpdec, "sink");
    gst_pad_link (pad, sinkpad);
    gst_object_unref (sinkpad);

}

bool linkStaticAndRequestPad(GstElement *sourse,GstElement *sink,gchar *nameSrcPad,gchar *nameSinkPad)
{

    GstPad *srcPad = gst_element_get_static_pad(sourse,nameSrcPad);
    GstPad *sinkPad = gst_element_get_request_pad(sink,nameSinkPad);
    GstPadLinkReturn ret_link = gst_pad_link(srcPad,sinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, beetwen recvRtpSinkPad and udpSrcRtpPad\n";
        return false;
    }
    gst_object_unref(GST_OBJECT(srcPad));
    gst_object_unref(GST_OBJECT(sinkPad));
    return true;
}
bool linkRequestAndStatic(GstElement *sourse,GstElement *sink,gchar *nameSrcPad,gchar *nameSinkPad)
{

    GstPad *srcPad = gst_element_get_request_pad(sourse,nameSrcPad);
    GstPad *sinkPad = gst_element_get_static_pad(sink,nameSinkPad);
    GstPadLinkReturn ret_link = gst_pad_link(srcPad,sinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, beetwen recvRtpSinkPad and udpSrcRtpPad\n";
        return false;
    }
    gst_object_unref(GST_OBJECT(srcPad));
    gst_object_unref(GST_OBJECT(sinkPad));
    return true;
}

static gboolean process_rtcp_packet(GstRTCPPacket *packet){
    guint32 ssrc, rtptime, packet_count, octet_count;
    guint64 ntptime;
    guint count, i;

    count = gst_rtcp_packet_get_rb_count(packet);
    cerr << "    count " << count;
    for (i=0; i<count; i++) {
        guint32 exthighestseq, jitter, lsr, dlsr;
        guint8 fractionlost;
        gint32 packetslost;

        gst_rtcp_packet_get_rb(packet, i, &ssrc, &fractionlost,
                               &packetslost, &exthighestseq, &jitter, &lsr, &dlsr);

        cerr << "    block " << i;
        cerr << "    ssrc " << ssrc;
        cerr << "    highest seq " << exthighestseq;
        cerr << "    jitter " << jitter;
        cerr << "    fraction lost " << fractionlost;
        cerr << "    packet lost " << packetslost;
        cerr << "    lsr " << lsr;
        cerr << "    dlsr " << dlsr;

        //        rtcp_pkt->fractionlost = fractionlost;
        //        rtcp_pkt->jitter = jitter;
        //        rtcp_pkt->packetslost = packetslost;
    }

    //cerr << "Received rtcp packet");

    return TRUE;
}


static gboolean cb_receive_rtcp(GstElement *rtpsession, GstBuffer *buf, gpointer data){

    GstRTCPBuffer rtcpBuffer = GST_RTCP_BUFFER_INIT;
    //    GstRTCPBuffer *rtcpBuffer = (GstRTCPBuffer*)malloc(sizeof(GstRTCPBuffer));
    //    rtcpBuffer->buffer = nullptr;
    GstRTCPPacket *rtcpPacket = (GstRTCPPacket*)malloc(sizeof(GstRTCPPacket));


    if (!gst_rtcp_buffer_validate(buf))
    {
        cerr << "Received invalid RTCP packet" << endl;
    }

    cerr << "Received rtcp packet" << "\n";


    gst_rtcp_buffer_map (buf,(GstMapFlags)(GST_MAP_READ),&rtcpBuffer);
    gboolean more = gst_rtcp_buffer_get_first_packet(&rtcpBuffer,rtcpPacket);
    while (more) {
        GstRTCPType type;

        type = gst_rtcp_packet_get_type(rtcpPacket);
        switch (type) {
        case GST_RTCP_TYPE_SR:
            process_rtcp_packet(rtcpPacket);
            //   gst_rtcp_buffer_unmap (&rtcpBuffer);
            //g_debug("RR");
            //send_event_to_encoder(venc, &rtcp_pkt);
            break;
        default:
            cerr << "Other types" << endl;
            break;
        }
        more = gst_rtcp_packet_move_to_next(rtcpPacket);
    }

    free(rtcpPacket);
    return TRUE;
}

GstElement *create_pipeline(){

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
    g_object_set(G_OBJECT(udpSrcRtp),"caps",gst_caps_from_string("application/x-rtp,media=(string)video,clock-rate=(int)90000,encoding-name=(string)H264,payload=(int)103"),NULL);
    g_object_set(G_OBJECT(udpSrcRtp),"port",5000,NULL);


    // Устанавливаю параметры для upd сойденений.
    g_object_set(G_OBJECT(udpSrcRtcp),"address","127.0.0.1",NULL);
    g_object_set(G_OBJECT(udpSrcRtcp),"port",5001,NULL);

    g_object_set(G_OBJECT(udpSinkRtcp),"host","127.0.0.1",NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"port",5005,NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"sync",FALSE,NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"async",FALSE,NULL);

    // Добавляю элементы в контейнер
    gst_bin_add_many(GST_BIN(pipeline),udpSrcRtp,udpSrcRtcp,rtpbin,udpSinkRtcp,
                     rtph264depay,x264decoder,videconverter,xvimagesink,NULL);

    // Сойденяю PADы.

    if (!linkStaticAndRequestPad(udpSrcRtp,rtpbin,"src","recv_rtp_sink_%u"))
    {
        cerr << "Error create link, beetwen udpSrcRtp and rtpbin\n";
        return NULL;
    }



    if (!linkStaticAndRequestPad(udpSrcRtcp,rtpbin,"src","recv_rtcp_sink_%u"))
    {
        cerr << "Error create link, beetwen udpSrcRtcp and rtpbin\n";
        return NULL;
    }



    if (!linkRequestAndStatic(rtpbin,udpSinkRtcp,"send_rtcp_src_%u","sink"))
    {
        cerr << "Error create link, beetwen rtpbin and udpSinkRtcp\n";
        return NULL;
    }


    g_signal_connect (rtpbin, "pad-added", G_CALLBACK (cb_new_rtp_recv_src_pad),rtph264depay );

    if (!gst_element_link_many(rtph264depay,x264decoder,videconverter,xvimagesink,NULL))
    {
        cerr << "Elements could not be linked other.\n";
        return NULL;

    }


    GObject *session;


    g_signal_emit_by_name (rtpbin, "get-internal-session", 0, &session);
    g_signal_connect_after (session, "on-receiving-rtcp",
                           G_CALLBACK (cb_receive_rtcp), NULL);
     g_object_unref(session);

    return pipeline;

}
int main()
{

    gst_init(0,0);

    GMainLoop *loop;
    loop = g_main_loop_new(NULL,FALSE);
    GstBus *bus;

    GstElement *pipeline = create_pipeline();
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

    return 0;
}
