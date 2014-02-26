/*
 *  Image Analyzer: Application object
 *  Copyright (C) 2007-2012 Rok Mandeljc
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <mirage.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "image-analyzer-application.h"
#include "image-analyzer-application-private.h"

#include "image-analyzer-log-window.h"
#include "image-analyzer-sector-analysis.h"
#include "image-analyzer-sector-read.h"
#include "image-analyzer-disc-structure.h"
#include "image-analyzer-disc-topology.h"
#include "image-analyzer-writer-dialog.h"

#include "image-analyzer-dump.h"
#include "image-analyzer-xml-tags.h"


/**********************************************************************\
 *                      Debug and logging redirection                 *
\**********************************************************************/
static void capture_log (const gchar *log_domain G_GNUC_UNUSED, GLogLevelFlags log_level, const gchar *message, IaApplication *self)
{
    /* Append to log */
    ia_log_window_append_to_log(IA_LOG_WINDOW(self->priv->dialog_log), message);

    /* Errors, critical errors and warnings are always printed to stdout */
    if (log_level & G_LOG_LEVEL_ERROR) {
        g_print("ERROR: %s", message);
        return;
    }
    if (log_level & G_LOG_LEVEL_CRITICAL) {
        g_print("CRITICAL: %s", message);
        return;
    }
    if (log_level & G_LOG_LEVEL_WARNING) {
        g_print("WARNING: %s", message);
        return;
    }

    /* Debug messages are printed to stdout only if user requested so */
    if (self->priv->debug_to_stdout) {
        g_print("%s", message);
    }
}

static void ia_application_set_debug_to_stdout (IaApplication *self, gboolean enabled)
{
    /* Debug flag */
    self->priv->debug_to_stdout = enabled;

    /* Set to GUI (libMirage log window) */
    ia_log_window_set_debug_to_stdout(IA_LOG_WINDOW(self->priv->dialog_log), enabled);
}

static void ia_application_set_debug_mask (IaApplication *self, gint debug_mask)
{
    /* Debug mask */
    mirage_context_set_debug_mask(self->priv->mirage_context, debug_mask);
}

static void ia_application_change_debug_mask (IaApplication *self)
{
    GtkWidget *dialog, *content_area;
    GtkWidget *vbox;
    GtkWidget **entries = NULL;
    gint mask;

    const MirageDebugMaskInfo *valid_masks;
    gint num_valid_masks;

    /* Get list of supported debug masks */
    mirage_get_supported_debug_masks(&valid_masks, &num_valid_masks, NULL);

    /* Get mask from debug context */
    mask = mirage_context_get_debug_mask(self->priv->mirage_context);

    /* Construct dialog */
    dialog = gtk_dialog_new_with_buttons("Debug mask",
                                         GTK_WINDOW(self->priv->dialog_log),
                                         GTK_DIALOG_MODAL,
                                         "OK", GTK_RESPONSE_ACCEPT,
                                         "Cancel", GTK_RESPONSE_REJECT,
                                         NULL);

    /* Create the mask widgets */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content_area), vbox);

    entries = g_new(GtkWidget *, num_valid_masks);
    for (gint i = 0; i < num_valid_masks; i++) {
        entries[i] = gtk_check_button_new_with_label(valid_masks[i].name);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entries[i]), mask & valid_masks[i].value);
        gtk_box_pack_start(GTK_BOX(vbox), entries[i], FALSE, FALSE, 0);
    }

    gtk_widget_show_all(vbox);

    /* Run the dialog */
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        mask = 0;

        for (gint i = 0; i < num_valid_masks; i++) {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entries[i]))) {
                mask |= valid_masks[i].value;
            }
        }

        /* Set the mask */
        mirage_context_set_debug_mask(self->priv->mirage_context, mask);
    }

    /* Destroy dialog */
    gtk_widget_destroy(dialog);
    g_free(entries);
}


/**********************************************************************\
 *                              Status message                        *
\**********************************************************************/
static void ia_application_message (IaApplication *self, gchar *format, ...)
{
    gchar *message;
    va_list args;

    /* Pop message (so that anything set previously will be removed */
    gtk_statusbar_pop(GTK_STATUSBAR(self->priv->statusbar), self->priv->context_id);

    /* Push message */
    va_start(args, format);
    message = g_strdup_vprintf(format, args);
    va_end(args);

    gtk_statusbar_pop(GTK_STATUSBAR(self->priv->statusbar), self->priv->context_id);
    gtk_statusbar_push(GTK_STATUSBAR(self->priv->statusbar), self->priv->context_id, message);

    g_free(message);
}


/**********************************************************************\
 *                           Open/close image                         *
\**********************************************************************/
static gchar *ia_application_get_password (IaApplication *self)
{
    gchar *password;
    GtkDialog *dialog;
    GtkWidget *hbox, *entry, *label;
    gint result;

    dialog = GTK_DIALOG(gtk_dialog_new_with_buttons(
        "Enter password",
        GTK_WINDOW(self->priv->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "OK", GTK_RESPONSE_OK,
        "Cancel", GTK_RESPONSE_REJECT,
        NULL));
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(dialog)), 5);

    label = gtk_label_new("The image you are trying to load is encrypted.");
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(dialog)), label, TRUE, TRUE, 0);

    entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Password: "), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(dialog)), hbox, FALSE, FALSE, 0);

    /* Run dialog */
    gtk_widget_show_all(GTK_WIDGET(dialog));
    result = gtk_dialog_run(dialog);
    switch (result) {
        case GTK_RESPONSE_OK: {
            password = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
            break;
        }
        default: {
            password = NULL;
            break;
        }
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));

    return password;
}

static gboolean ia_application_close_image_or_dump (IaApplication *self)
{
    /* Clear disc reference in child windows */
    ia_disc_structure_set_disc(IA_DISC_STRUCTURE(self->priv->dialog_structure), NULL);
    ia_disc_topology_set_disc(IA_DISC_TOPOLOGY(self->priv->dialog_topology), NULL);
    ia_sector_read_set_disc(IA_SECTOR_READ(self->priv->dialog_sector), NULL);
    ia_sector_analysis_set_disc(IA_SECTOR_ANALYSIS(self->priv->dialog_analysis), NULL);

    /* Clear TreeStore */
    gtk_tree_store_clear(self->priv->treestore);

    /* Free XML doc */
    if (self->priv->xml_doc) {
        xmlFreeDoc(self->priv->xml_doc);
        self->priv->xml_doc = NULL;
    }

    /* Release disc reference */
    if (self->priv->disc) {
        g_object_unref(self->priv->disc);
        self->priv->disc = NULL;
    }

    /* Print message only if something was loaded */
    if (self->priv->loaded) {
        ia_application_message(self, "Image/dump closed.");
    }

    /* Clear the log */
    ia_log_window_clear_log(IA_LOG_WINDOW(self->priv->dialog_log));

    self->priv->loaded = FALSE;

    return TRUE;
}

static gboolean ia_application_open_image (IaApplication *self, gchar **filenames)
{
    GError *error = NULL;

    /* Close any opened image or dump */
    ia_application_close_image_or_dump(self);

    /* Load image */
    self->priv->disc = mirage_context_load_image(self->priv->mirage_context, filenames, &error);
    if (!self->priv->disc) {
        g_warning("Failed to create disc: %s\n", error->message);
        ia_application_message(self, "Failed to open image: %s", error->message);
        g_error_free(error);
        return FALSE;
    }

    /* Set disc reference in child windows */
    ia_disc_structure_set_disc(IA_DISC_STRUCTURE(self->priv->dialog_structure), self->priv->disc);
    ia_disc_topology_set_disc(IA_DISC_TOPOLOGY(self->priv->dialog_topology), self->priv->disc);
    ia_sector_read_set_disc(IA_SECTOR_READ(self->priv->dialog_sector), self->priv->disc);
    ia_sector_analysis_set_disc(IA_SECTOR_ANALYSIS(self->priv->dialog_analysis), self->priv->disc);

    /* Make XML dump */
    ia_application_create_xml_dump(self);

    /* Display XML */
    ia_application_display_xml_data(self);

    self->priv->loaded = TRUE;

    ia_application_message(self, "Image successfully opened.");

    return TRUE;
}

static gboolean ia_application_open_dump (IaApplication *self, gchar *filename)
{
    /* Close any opened image or dump */
    ia_application_close_image_or_dump(self);

    /* Load XML */
    self->priv->xml_doc = xmlReadFile(filename, NULL, 0);
    if (!self->priv->xml_doc) {
        g_warning("Failed to dump disc!\n");
        ia_application_message(self, "Failed to load dump!");
        return FALSE;
    }

    /* Display XML */
    ia_application_display_xml_data(self);

    self->priv->loaded = TRUE;

    ia_application_message(self, "Dump successfully opened.");

    return TRUE;
}

static void ia_application_save_dump (IaApplication *self, gchar *filename)
{
    /* Create a copy of XML dump */
    xmlDocPtr output_xml = xmlCopyDoc(self->priv->xml_doc, 1);

    if (output_xml) {
        /* Append the log */
        xmlNodePtr root_node = xmlDocGetRootElement(output_xml);
        gchar *log = ia_log_window_get_log_text(IA_LOG_WINDOW(self->priv->dialog_log));
        xmlNewTextChild(root_node, NULL, BAD_CAST TAG_LIBMIRAGE_LOG, BAD_CAST log);
        g_free(log);

        /* Save the XML tree */
        if (xmlSaveFormatFileEnc(filename, output_xml, "UTF-8", 1) == -1) {
            ia_application_message(self, "Saving to dump file failed!");
        } else {
            ia_application_message(self, "Dump successfully saved.");
        }

        /* Free */
        xmlFreeDoc(output_xml);
    } else {
        ia_application_message(self, "Failed to create dump!");
    }
}


/**********************************************************************\
 *                          Image conversion                          *
\**********************************************************************/
static void update_conversion_progress (GtkProgressBar *progress_bar, guint progress, MirageWriter *writer G_GNUC_UNUSED)
{
    /* Update progress bar */
    gtk_progress_bar_set_fraction(progress_bar, progress/100.0);

    /* Process events */
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
}

static void cancel_conversion (GCancellable *cancellable, gint response, GtkButton *button G_GNUC_UNUSED)
{
    if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT) {
        g_cancellable_cancel(cancellable);
    }
}


static void ia_application_convert_image (IaApplication *self, MirageWriter *writer, const gchar *filename, GHashTable *writer_parameters)
{
    GError *local_error = NULL;
    gboolean succeeded;

    /* Make sure writer has our context */
    mirage_contextual_set_context(MIRAGE_CONTEXTUAL(writer), self->priv->mirage_context);

    /* Create progess dialog */
    GtkWidget *progress_dialog = gtk_dialog_new();
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(progress_dialog));
    GtkWidget *progress_bar = gtk_progress_bar_new();

    g_signal_connect(progress_dialog, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL); /* We need this because we don't do "run" on the dialog */
    gtk_window_set_title(GTK_WINDOW(progress_dialog), "Image conversion progress");
    gtk_window_set_transient_for(GTK_WINDOW(progress_dialog), GTK_WINDOW(self->priv->window));
    gtk_window_set_modal(GTK_WINDOW(progress_dialog), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(progress_dialog), 5);
    gtk_dialog_add_button(GTK_DIALOG(progress_dialog), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_container_add(GTK_CONTAINER(content_area), progress_bar);
    gtk_widget_show_all(progress_dialog);

    /* Conversion cancelling support */
    GCancellable *cancellable = g_cancellable_new();
    g_signal_connect_object(progress_dialog, "response", G_CALLBACK(cancel_conversion), cancellable, G_CONNECT_SWAPPED);

    /* Set up writer's conversion progress reporting */
    mirage_writer_set_conversion_progress_step(writer, 5);
    g_signal_connect_object(writer, "conversion-progress", G_CALLBACK(update_conversion_progress), progress_bar, G_CONNECT_SWAPPED);

    /* Convert */
    succeeded = mirage_writer_convert_image(writer, filename, self->priv->disc, writer_parameters, cancellable, &local_error);
    g_object_unref(cancellable);

    /* Destroy progress dialog */
    gtk_widget_destroy(progress_dialog);

    /* Display success/error message */
    GtkWidget *message_dialog;
    if (!succeeded) {
        message_dialog = gtk_message_dialog_new(
            GTK_WINDOW(self->priv->window),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            local_error->message);

        g_error_free(local_error);
    } else {
        message_dialog = gtk_message_dialog_new(
            GTK_WINDOW(self->priv->window),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_CLOSE,
            "Image conversion succeeded.");
    }

    gtk_dialog_run(GTK_DIALOG(message_dialog));
    gtk_widget_destroy(message_dialog);
}


/**********************************************************************\
 *                             UI callbacks                           *
\**********************************************************************/
static void ui_callback_open_image (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* Run the dialog */
    if (gtk_dialog_run(GTK_DIALOG(self->priv->dialog_open_image)) == GTK_RESPONSE_ACCEPT) {
        GSList *filenames_list;
        GSList *entry;
        gint num_filenames;
        gchar **filenames;
        gint i = 0;

        /* Get filenames from dialog */
        filenames_list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(self->priv->dialog_open_image));

        /* Create strings array */
        num_filenames = g_slist_length(filenames_list);
        filenames = g_new0(gchar *, num_filenames + 1); /* NULL-terminated */

        /* GSList -> strings array conversion */
        entry = filenames_list;
        while (entry != NULL) {
            filenames[i++] = entry->data;
            entry = g_slist_next(entry);
        }

        /* Open image */
        ia_application_open_image(self, filenames);

        /* Free filenames list */
        g_slist_free(filenames_list);
        /* Free filenames array */
        g_strfreev(filenames);
    }

    gtk_widget_hide(GTK_WIDGET(self->priv->dialog_open_image));
}

static void ui_callback_convert_image (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* We need an opened image for this */
    if (!self->priv->disc) {
        return;
    }

    /* Run the image writer dialog */
    MirageWriter *writer;
    GHashTable *writer_parameters;
    const gchar *filename;

    gint response;

    while (TRUE) {
        response = gtk_dialog_run(GTK_DIALOG(self->priv->dialog_image_writer));

        if (response == GTK_RESPONSE_ACCEPT) {
            /* Validate filename */
            filename = ia_writer_dialog_get_filename(IA_WRITER_DIALOG(self->priv->dialog_image_writer));
            if (!filename || !strlen(filename)) {
                GtkWidget *message_dialog = gtk_message_dialog_new(
                    GTK_WINDOW(self->priv->dialog_image_writer),
                    GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_CLOSE,
                    "Image filename/basename not set!");

                gtk_dialog_run(GTK_DIALOG(message_dialog));
                gtk_widget_destroy(message_dialog);
                continue;
            }

            /* Validate writer */
            writer = ia_writer_dialog_get_writer(IA_WRITER_DIALOG(self->priv->dialog_image_writer));
            if (!writer) {
                GtkWidget *message_dialog = gtk_message_dialog_new(
                    GTK_WINDOW(self->priv->dialog_image_writer),
                    GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_CLOSE,
                    "No image writer is chosen!");

                gtk_dialog_run(GTK_DIALOG(message_dialog));
                gtk_widget_destroy(message_dialog);
                continue;
            }

            /* Get writer parameters */
            writer_parameters = ia_writer_dialog_get_writer_parameters(IA_WRITER_DIALOG(self->priv->dialog_image_writer));

            break;
        } else {
            break;
        }
    }

    /* Hide the dialog */
    gtk_widget_hide(GTK_WIDGET(self->priv->dialog_image_writer));

    /* Conversion */
    if (response == GTK_RESPONSE_ACCEPT) {
        /* Convert */
        ia_application_convert_image(self, writer, filename, writer_parameters);
        /* Cleanup */
        g_object_unref(writer);
        g_hash_table_unref(writer_parameters);
    }
}

static void ui_callback_open_dump (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* Run the dialog */
    if (gtk_dialog_run(GTK_DIALOG(self->priv->dialog_open_dump)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename;

        /* Get filenames from dialog */
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(self->priv->dialog_open_dump));

        /* Open dump */
        ia_application_open_dump(self, filename);

        /* Free filename */
        g_free(filename);
    }

    gtk_widget_hide(GTK_WIDGET(self->priv->dialog_open_dump));
}

static void ui_callback_save_dump (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* We need an opened image or dump for this */
    if (!self->priv->loaded) {
        return;
    }

    /* Run the dialog */
    if (gtk_dialog_run(GTK_DIALOG(self->priv->dialog_save_dump)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename;

        /* Get filenames from dialog */
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(self->priv->dialog_save_dump));

        /* Save */
        ia_application_save_dump(self, filename);

        /* Free filename */
        g_free(filename);
    }

    gtk_widget_hide(GTK_WIDGET(self->priv->dialog_save_dump));
}

static void ui_callback_close (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    ia_application_close_image_or_dump(self);
}

static void ui_callback_quit (GtkAction *action G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
    gtk_main_quit();
}


static void ui_callback_log_window (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* Make window (re)appear by first hiding, then showing it */
    gtk_widget_hide(self->priv->dialog_log);
    gtk_widget_show_all(self->priv->dialog_log);
}

static void ui_callback_sector (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* Make window (re)appear by first hiding, then showing it */
    gtk_widget_hide(self->priv->dialog_sector);
    gtk_widget_show_all(self->priv->dialog_sector);
}

static void ui_callback_analysis (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* Make window (re)appear by first hiding, then showing it */
    gtk_widget_hide(self->priv->dialog_analysis);
    gtk_widget_show_all(self->priv->dialog_analysis);
}

static void ui_callback_topology (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* Make window (re)appear by first hiding, then showing it */
    gtk_widget_hide(self->priv->dialog_topology);
    gtk_widget_show_all(self->priv->dialog_topology);
}

static void ui_callback_structure (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    /* Make window (re)appear by first hiding, then showing it */
    gtk_widget_hide(self->priv->dialog_structure);
    gtk_widget_show_all(self->priv->dialog_structure);
}

static void ui_callback_about (GtkAction *action G_GNUC_UNUSED, IaApplication *self)
{
    gchar *authors[] = { "Rok Mandeljc <rok.mandeljc@gmail.com>", NULL };

    gtk_show_about_dialog(
        GTK_WINDOW(self->priv->window),
        "name", "Image Analyzer",
        "comments", "Image Analyzer displays tree structure of disc image created by libMirage.",
        "version", IMAGE_ANALYZER_VERSION,
        "authors", authors,
        "copyright", "Copyright (C) 2007-2012 Rok Mandeljc",
        NULL);

    return;
}


static gboolean cb_window_delete_event (GtkWidget *widget G_GNUC_UNUSED, GdkEvent *event G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
    /* Quit the app */
    gtk_main_quit();
    /* Don't invoke other handlers, we'll cleanup stuff ourselves */
    return TRUE;
}



static void callback_debug_to_stdout_change_requested (IaLogWindow *log_window G_GNUC_UNUSED, gboolean value, IaApplication *self)
{
    /* Set the new value */
    ia_application_set_debug_to_stdout(self, value);
}

static void callback_debug_mask_change_requested (IaLogWindow *log_window G_GNUC_UNUSED, IaApplication *self)
{
    /* Propagate to helper function */
    ia_application_change_debug_mask(self);
}


/**********************************************************************\
 *                              GUI setup                             *
\**********************************************************************/
static GtkWidget *build_dialog_open_image (GtkWindow *parent_window)
{
    GtkWidget *dialog;
    GtkFileFilter *filter_all;

    const MirageParserInfo *parsers;
    gint num_parsers;

    const MirageFilterStreamInfo *filter_streams;
    gint num_filter_streams;

    dialog = gtk_file_chooser_dialog_new(
        "Open File",
        parent_window,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), FALSE);

    /* "All files" filter */
    filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "All files");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    /* "All images" filter */
    filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "All images");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    /* Get list of supported parsers */
    mirage_get_parsers_info(&parsers, &num_parsers, NULL);
    for (gint i = 0; i < num_parsers; i++) {
        const MirageParserInfo *info = &parsers[i];
        GtkFileFilter *filter;

        /* Go over all types (NULL-terminated list) */
        for (gint j = 0; info->description[j]; j++) {
            /* Create a parser-specific file chooser filter */
            filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, info->description[j]);
            gtk_file_filter_add_mime_type(filter, info->mime_type[j]);
            gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

            /* "All images" filter */
            gtk_file_filter_add_mime_type(filter_all, info->mime_type[j]);
        }
    }

    /* Get list of supported file filters */
    mirage_get_filter_streams_info(&filter_streams, &num_filter_streams, NULL);
    for (gint i = 0; i < num_filter_streams; i++) {
        const MirageFilterStreamInfo *info = &filter_streams[i];
        GtkFileFilter *filter;

        /* Go over all types (NULL-terminated list) */
        for (gint j = 0; info->description[j]; j++) {
            /* Create a parser-specific file chooser filter */
            filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, info->description[j]);
            gtk_file_filter_add_mime_type(filter, info->mime_type[j]);
            gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

            /* "All images" filter */
            gtk_file_filter_add_mime_type(filter_all, info->mime_type[j]);
        }
    }

    return dialog;
}

static GtkWidget *build_dialog_open_dump (GtkWindow *parent_window)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;

    dialog = gtk_file_chooser_dialog_new(
        "Open File",
        parent_window,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), FALSE);

    /* "XML files" filter */
    filter= gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "XML files");
    gtk_file_filter_add_pattern(filter, "*.xml");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    /* "All files" filter */
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "All files");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    return dialog;
}

static GtkWidget *build_dialog_save_dump (GtkWindow *parent_window)
{
    GtkWidget *dialog;
    GtkFileFilter *filter;

    dialog = gtk_file_chooser_dialog_new(
        "Save File",
        parent_window,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), FALSE);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    /* "XML files" filter */
    filter= gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "XML files");
    gtk_file_filter_add_pattern(filter, "*.xml");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    /* "All files" filter */
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "All files");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    return dialog;
}

static const GtkActionEntry menu_entries[] = {
    { "FileMenuAction", NULL, "_File", NULL, NULL, NULL },
    { "ImageMenuAction", NULL, "_Image", NULL, NULL, NULL },
    { "HelpMenuAction", NULL, "_Help", NULL, NULL, NULL },

    { "OpenImageAction", GTK_STOCK_OPEN, "_Open image", "<control>O", "Open image", G_CALLBACK(ui_callback_open_image) },
    { "ConvertImageAction", GTK_STOCK_SAVE, "_Convert image", "<control>C", "Convert image", G_CALLBACK(ui_callback_convert_image) },
    { "OpenDumpAction", GTK_STOCK_OPEN, "Open _dump", "<control>D", "Open dump", G_CALLBACK(ui_callback_open_dump) },
    { "SaveDumpAction", GTK_STOCK_SAVE, "_Save dump", "<control>S", "Save dump", G_CALLBACK(ui_callback_save_dump) },
    { "CloseAction", GTK_STOCK_CLOSE, "_Close", "<control>W", "Close", G_CALLBACK(ui_callback_close) },
    { "QuitAction", GTK_STOCK_QUIT, "_Quit", "<control>Q", "Quit", G_CALLBACK(ui_callback_quit) },

    { "LogAction", GTK_STOCK_DIALOG_INFO, "libMirage _log", "<control>L", "libMirage log", G_CALLBACK(ui_callback_log_window) },
    { "SectorAction", GTK_STOCK_EXECUTE, "_Read sector", "<control>R", "Read sector", G_CALLBACK(ui_callback_sector) },
    { "AnalysisAction", GTK_STOCK_EXECUTE, "Sector _Analysis", "<control>A", "Sector analysis", G_CALLBACK(ui_callback_analysis) },
    { "TopologyAction", GTK_STOCK_EXECUTE, "Disc _topology", "<control>T", "Disc topology", G_CALLBACK(ui_callback_topology) },
    { "StructureAction", GTK_STOCK_EXECUTE, "Disc stru_cture", "<control>C", "Disc structures", G_CALLBACK(ui_callback_structure) },

    { "AboutAction", GTK_STOCK_ABOUT, "_About", NULL, "About", G_CALLBACK(ui_callback_about) },
};

static const gchar menu_xml[] =
    "<ui>"
    "   <menubar name='MenuBar'>"
    "       <menu name='FileMenu' action='FileMenuAction'>"
    "           <menuitem name='Open image' action='OpenImageAction' />"
    "           <menuitem name='Convert image' action='ConvertImageAction' />"
    "           <separator/>"
    "           <menuitem name='Open dump' action='OpenDumpAction' />"
    "           <menuitem name='Save dump' action='SaveDumpAction' />"
    "           <separator/>"
    "           <menuitem name='Close' action='CloseAction' />"
    "           <menuitem name='Quit' action='QuitAction' />"
    "       </menu>"
    "       <menu name='Image' action='ImageMenuAction'>"
    "           <menuitem name='libMirage log' action='LogAction' />"
    "           <menuitem name='Read sector' action='SectorAction' />"
    "           <menuitem name='Sector analysis' action='AnalysisAction' />"
    "           <menuitem name='Disc topology' action='TopologyAction' />"
    "           <menuitem name='Disc structures' action='StructureAction' />"
    "       </menu>"
    "       <menu name='HelpMenu' action='HelpMenuAction'>"
    "           <menuitem name='About' action='AboutAction' />"
    "       </menu>"
    "   </menubar>"
    "</ui>";


static void create_gui (IaApplication *self)
{
    GtkWidget *vbox;

    /* Window */
    self->priv->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(self->priv->window, "delete_event", G_CALLBACK(cb_window_delete_event), self);
    gtk_window_set_title(GTK_WINDOW(self->priv->window), "Image analyzer");
    gtk_window_set_default_size(GTK_WINDOW(self->priv->window), 300, 400);
    gtk_container_set_border_width(GTK_CONTAINER(self->priv->window), 5);

    /* VBox */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(self->priv->window), vbox);

    /* Menubar, actions and accelerators */
    GtkUIManager *ui_manager = gtk_ui_manager_new();
    GError *error = NULL;

    gtk_ui_manager_add_ui_from_string(ui_manager, menu_xml, -1, &error);
    if (error) {
        g_warning("Building menus failed: %s", error->message);
        g_error_free(error);
    }

    GtkActionGroup *actiongroup = gtk_action_group_new("Image Analyzer");
    gtk_action_group_add_actions(actiongroup, menu_entries, G_N_ELEMENTS(menu_entries), self);
    gtk_ui_manager_insert_action_group(ui_manager, actiongroup, 0);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_ui_manager_get_widget(ui_manager, "/MenuBar"), FALSE, FALSE, 0);

    gtk_window_add_accel_group(GTK_WINDOW(self->priv->window), gtk_ui_manager_get_accel_group(ui_manager));

    g_object_unref(ui_manager);

    /* Scrolled window */
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    /* Tree store and view widget */
    self->priv->treestore = gtk_tree_store_new(1, G_TYPE_STRING);

    GtkWidget *treeview = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(self->priv->treestore));

    gtk_container_add(GTK_CONTAINER(scrolled_window), treeview);

    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", 0);

    /* Status bar */
    self->priv->statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), self->priv->statusbar, FALSE, FALSE, 0);
    self->priv->context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(self->priv->statusbar), "Message");

    /* Load/save dialogs */
    self->priv->dialog_open_image = build_dialog_open_image(GTK_WINDOW(self->priv->window));
    self->priv->dialog_open_dump = build_dialog_open_dump(GTK_WINDOW(self->priv->window));
    self->priv->dialog_save_dump = build_dialog_save_dump(GTK_WINDOW(self->priv->window));

    /* Image writer dialog */
    self->priv->dialog_image_writer = g_object_new(IA_TYPE_WRITER_DIALOG, NULL);

    /* Mirage log dialog */
    self->priv->dialog_log = g_object_new(IA_TYPE_LOG_WINDOW, NULL);
    g_signal_connect(self->priv->dialog_log, "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    g_signal_connect(self->priv->dialog_log, "debug_to_stdout_change_requested", G_CALLBACK(callback_debug_to_stdout_change_requested), self);
    g_signal_connect(self->priv->dialog_log, "debug_mask_change_requested", G_CALLBACK(callback_debug_mask_change_requested), self);

    /* Sector read dialog */
    self->priv->dialog_sector = g_object_new(IA_TYPE_SECTOR_READ, NULL);
    g_signal_connect(self->priv->dialog_sector, "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    /* Sector analysis dialog */
    self->priv->dialog_analysis = g_object_new(IA_TYPE_SECTOR_ANALYSIS, NULL);
    g_signal_connect(self->priv->dialog_analysis, "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    /* Disc topology dialog */
    self->priv->dialog_topology = g_object_new(IA_TYPE_DISC_TOPOLOGY, NULL);
    g_signal_connect(self->priv->dialog_topology, "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    ia_disc_topology_set_disc(IA_DISC_TOPOLOGY(self->priv->dialog_topology), NULL);

    /* Disc structures dialog */
    self->priv->dialog_structure = g_object_new(IA_TYPE_DISC_STRUCTURE, NULL);
    g_signal_connect(self->priv->dialog_structure, "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    ia_disc_structure_set_disc(IA_DISC_STRUCTURE(self->priv->dialog_structure), NULL);
}


/**********************************************************************\
 *                             Public API                             *
\**********************************************************************/
gboolean ia_application_run (IaApplication *self, gchar **open_image, gboolean debug_to_stdout, gint debug_mask_initial)
{
    /* Set the debug flags and masks */
    ia_application_set_debug_to_stdout(self, debug_to_stdout);
    ia_application_set_debug_mask(self, debug_mask_initial);

    /* Open image, if provided */
    if (g_strv_length(open_image)) {
        /* If it ends with .xml, we treat it as a dump file */
        if (mirage_helper_has_suffix(open_image[0], ".xml")) {
            ia_application_open_dump(self, open_image[0]);
        } else {
            ia_application_open_image(self, open_image);
        }
    }

    /* Show window */
    gtk_widget_show_all(self->priv->window);

    /* GtkMain() */
    gtk_main();

    return TRUE;
}


/**********************************************************************\
 *                             Object init                            *
\**********************************************************************/
G_DEFINE_TYPE(IaApplication, ia_application, G_TYPE_OBJECT);

static void ia_application_init (IaApplication *self)
{
    self->priv = IA_APPLICATION_GET_PRIVATE(self);

    self->priv->disc = NULL;

    /* Setup libMirage context */
    self->priv->mirage_context = g_object_new(MIRAGE_TYPE_CONTEXT, NULL);
    mirage_context_set_debug_domain(self->priv->mirage_context, "libMirage");
    mirage_context_set_debug_mask(self->priv->mirage_context, MIRAGE_DEBUG_PARSER);
    mirage_context_set_password_function(self->priv->mirage_context, (MiragePasswordFunction)ia_application_get_password, self);

    /* Setup GUI */
    create_gui(self);

    /* Setup log handler */
    g_log_set_handler("libMirage", G_LOG_LEVEL_MASK, (GLogFunc)capture_log, self);
}

static void ia_application_finalize (GObject *gobject)
{
    IaApplication *self = IA_APPLICATION(gobject);

    /* Close image */
    ia_application_close_image_or_dump(self);

    if (self->priv->mirage_context) {
        g_object_unref(self->priv->mirage_context);
    }

    gtk_widget_destroy(self->priv->window);

    gtk_widget_destroy(self->priv->dialog_open_image);
    gtk_widget_destroy(self->priv->dialog_image_writer);
    gtk_widget_destroy(self->priv->dialog_open_dump);
    gtk_widget_destroy(self->priv->dialog_save_dump);
    gtk_widget_destroy(self->priv->dialog_log);
    gtk_widget_destroy(self->priv->dialog_sector);
    gtk_widget_destroy(self->priv->dialog_analysis);
    gtk_widget_destroy(self->priv->dialog_topology);
    gtk_widget_destroy(self->priv->dialog_structure);

    /* Chain up to the parent class */
    return G_OBJECT_CLASS(ia_application_parent_class)->finalize(gobject);
}

static void ia_application_class_init (IaApplicationClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = ia_application_finalize;

    /* Register private structure */
    g_type_class_add_private(klass, sizeof(IaApplicationPrivate));
}
