/*
 * Copyright (C) 2012 Nicolas Bouillot (http://www.nicolasbouillot.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

#include "shmdata/reader.h"
#include "shmdata/data-reader.h"
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappbuffer.h>
#include <gst/app/gstappsink.h>

struct shmdata_data_reader_ {
    shmdata_reader_t *reader_;
    //pipeline elements
    GstElement *pipeline_;
    gchar *type_;
    GstElement *funnel_;
    GstElement *sink_;
    int debug_;
    GMainLoop *loop_;
    GThread *sharedDataThread_;
    //user callback
    void(*on_data_)(shmdata_data_reader_t *,
		    void *, 
		    int, 
		    unsigned long long, 
		    const char *,
		    void *);
    void* on_data_user_data_;
    GstCaps *data_caps_;
};


void
shmdata_data_reader_log_handler (const gchar *log_domain, 
				 GLogLevelFlags log_level,
				 const gchar *message,
				 gpointer user_data)
{
    if ( g_strcmp0 (log_domain,G_LOG_DOMAIN) == 0 )
    {
	shmdata_data_reader_t *context = (shmdata_data_reader_t *)user_data;
	switch (log_level) {
	case G_LOG_LEVEL_ERROR:
	    if (context->debug_ == SHMDATA_DATA_READER_ENABLE_DEBUG) 
		g_print ("%s, ERROR: %s\n",G_LOG_DOMAIN,message);
	    break;
	case G_LOG_LEVEL_CRITICAL:
	    if (context->debug_ == SHMDATA_DATA_READER_ENABLE_DEBUG) 
		g_print ("%s, CRITICAL: %s\n",G_LOG_DOMAIN,message);
	    break;
	case G_LOG_LEVEL_WARNING:
	    if (context->debug_ == SHMDATA_DATA_READER_ENABLE_DEBUG) 
		g_print ("%s, WARNING: %s\n",G_LOG_DOMAIN,message);
	    break;
	case G_LOG_LEVEL_MESSAGE:
	    if (context->debug_ == SHMDATA_DATA_READER_ENABLE_DEBUG) 
		g_print ("%s, MESSAGE: %s\n",G_LOG_DOMAIN,message);
	    break;
	case G_LOG_LEVEL_INFO:
	    if (context->debug_ == SHMDATA_DATA_READER_ENABLE_DEBUG) 
		g_print ("%s, INFO: %s\n",G_LOG_DOMAIN,message);
	    break;
	case G_LOG_LEVEL_DEBUG:
	    if (context->debug_ == SHMDATA_DATA_READER_ENABLE_DEBUG) 
		g_print ("%s, DEBUG: %s\n",G_LOG_DOMAIN,message);
	    break;
	default:
	    if (context->debug_ == SHMDATA_DATA_READER_ENABLE_DEBUG) 
		g_print ("%s: %s\n",G_LOG_DOMAIN,message);
	    break;
	}
    }
}

static void
shmdata_data_reader_on_new_buffer_from_source (GstElement * elt, gpointer user_data)
{
    shmdata_data_reader_t *me = (shmdata_data_reader_t *)user_data;
    
    GstBuffer *buf;
    
    /* pull the next item, this can return NULL when there is no more data and
     * EOS has been received */
    buf = gst_app_sink_pull_buffer (GST_APP_SINK (me->sink_));
    
    if (me->on_data_ != NULL)
    {

	if (me->type_ == NULL || gst_caps_is_always_compatible (me->data_caps_,GST_BUFFER_CAPS(buf)))
	 { 
	     me->on_data_ (me, 
	 		  (void *)GST_BUFFER_DATA(buf), 
	 		  (int)GST_BUFFER_SIZE(buf), 
	 		  (unsigned long)GST_TIME_AS_MSECONDS(GST_BUFFER_TIMESTAMP(buf)), 
	 		  (const char *)gst_caps_to_string(GST_BUFFER_CAPS(buf)), 
	 		  (void *)me->on_data_user_data_); 
	    
	 } 
	 else 
	 { 
	       g_warning ("incompatible data frame retrieved, data %p, data size %d, timestamp %llu, caps %s",     
	        	       GST_BUFFER_DATA(buf),     
	        	       GST_BUFFER_SIZE(buf),    
	        	       GST_TIME_AS_MSECONDS(GST_BUFFER_TIMESTAMP(buf)),    
	        	       gst_caps_to_string(GST_BUFFER_CAPS(buf)));    
	    
	 } 
	
    }

    if (buf)
	gst_buffer_unref (buf);
}

void
shmdata_data_reader_on_first_video_data (shmdata_reader_t *context, void *user_data)
{
    shmdata_data_reader_t *me = (shmdata_data_reader_t *)user_data;

    g_debug ("on first data received");
    me->funnel_ = gst_element_factory_make ("funnel", NULL);
    if (me->funnel_ == NULL)
        g_critical ("cannot create gstreamer element (funnel)");
    me->sink_ = gst_element_factory_make ("appsink", NULL);
    if (me->sink_ == NULL)
        g_critical ("cannot create gstreamer element (appsink)");
    g_object_set (G_OBJECT (me->sink_), 
		  "emit-signals", TRUE, 
		  "sync", FALSE, 
		  NULL);
    g_signal_connect (me->sink_, 
		      "new-buffer",
     		      G_CALLBACK (shmdata_data_reader_on_new_buffer_from_source), 
		      me);
    
    gst_element_set_state (me->funnel_, GST_STATE_PLAYING);
    gst_element_set_state (me->sink_, GST_STATE_PLAYING);
    gst_bin_add_many (GST_BIN (me->pipeline_), 
		      me->funnel_, 
		      me->sink_, 
		      NULL);
    gst_element_link (me->funnel_, me->sink_);
    //now tells the shared data reader where to write the data
    shmdata_reader_set_sink (context, me->funnel_);
}

shmdata_data_reader_t *
shmdata_data_reader_init (const char *socketName)
{
    shmdata_data_reader_t *reader = (shmdata_data_reader_t *)g_malloc0 (sizeof(shmdata_data_reader_t));
    reader->debug_ = SHMDATA_DATA_READER_DISABLE_DEBUG;
    g_log_set_default_handler (shmdata_data_reader_log_handler,
			       reader);
    reader->on_data_ = NULL;
    reader->on_data_user_data_ = NULL;
    reader->type_ = NULL;
    reader->data_caps_ = NULL;

    gst_init (NULL, NULL);
    reader->loop_ = g_main_loop_new (NULL, FALSE);
    reader->pipeline_ = gst_pipeline_new (NULL);
    if (reader->pipeline_ == NULL)
        g_critical ("cannot create gstreamer pipeline");
    gst_element_set_state (reader->pipeline_, GST_STATE_PLAYING);

    return reader;
}

void 
shmdata_data_reader_set_debug (shmdata_data_reader_t *context,int debug)
{
    context->debug_ = debug;
}

void shmdata_data_reader_set_on_data_handler (shmdata_data_reader_t *reader,
					      void(*on_data)(shmdata_data_reader_t *,
							     void *, 
							     int, 
							     unsigned long long, 
							     const char *,
							     void *), 
					      void *user_data)
{
    reader->on_data_ = on_data;
    reader->on_data_user_data_ = user_data;
}


void
shmdata_data_reader_g_loop_thread (gpointer user_data)
{
    shmdata_data_reader_t *context = (shmdata_data_reader_t *)user_data;
    g_main_loop_run (context->loop_);
}


void 
shmdata_data_reader_start (shmdata_data_reader_t *context,const char *socketName)
{
    //initializing lower layer library
    context->reader_ = shmdata_reader_init (socketName, 
					    context->pipeline_,
					    &shmdata_data_reader_on_first_video_data,
					    context);
    g_thread_init (NULL);
    context->sharedDataThread_ = g_thread_create ((GThreadFunc) shmdata_data_reader_g_loop_thread, 
					  context, 
					  FALSE, 
					  NULL);

//    g_main_loop_run (context->loop_);
}

void 
shmdata_data_reader_set_data_type(shmdata_data_reader_t *reader,const char *type)
{
     reader->type_ = g_strdup (type); 
     reader->data_caps_ = gst_caps_from_string (reader->type_);
}
     
void 
shmdata_data_reader_close(shmdata_data_reader_t *reader)
{
    shmdata_reader_close (reader->reader_);
    gst_caps_unref (reader->data_caps_);
    g_free (reader->type_);
    g_free (reader);
}


