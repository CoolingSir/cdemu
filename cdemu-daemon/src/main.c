/*
 *  CDEmu daemon: main
 *  Copyright (C) 2006-2014 Rok Mandeljc
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

#include <glib-unix.h>
#include "cdemu.h"

CdemuDaemon *daemon_obj;
FILE *logfile;

static gint num_devices = 1;
static gchar *ctl_device = "/dev/vhba_ctl";
static gchar *audio_driver = "null";
static gchar *bus = "session";
static gchar *log_filename = NULL;
static guint cdemu_debug_mask = 0;
static guint mirage_debug_mask = 0;

static GOptionEntry option_entries[] = {
    { "num-devices", 'n', 0, G_OPTION_ARG_INT, &num_devices, N_("Number of devices"), N_("N") },
    { "ctl-device", 'c', 0, G_OPTION_ARG_STRING, &ctl_device, N_("Control device"), N_("path") },
    { "audio-driver", 'a', 0, G_OPTION_ARG_STRING, &audio_driver, N_("Audio driver"), N_("driver") },
    { "bus", 'b', 0, G_OPTION_ARG_STRING, &bus, N_("Bus type to use"), N_("bus_type") },
    { "logfile", 'l', 0, G_OPTION_ARG_STRING, &log_filename, N_("Logfile"), N_("logfile") },
    { "default-cdemu-debug-mask", 0, 0, G_OPTION_ARG_INT, &cdemu_debug_mask, N_("Default debug mask for CDEmu devices"), N_("mask") },
    { "default-mirage-debug-mask", 0, 0, G_OPTION_ARG_INT, &mirage_debug_mask, N_("Default debug mask for underlying libMirage"), N_("mask") },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};


/* Log handler: writing to stdout */
static void log_handler_stdout (const gchar *log_domain G_GNUC_UNUSED, GLogLevelFlags log_level G_GNUC_UNUSED, const gchar *message, gpointer unused_data G_GNUC_UNUSED)
{
    g_print("%s", message);
}

static void log_handler_logfile (const gchar *log_domain G_GNUC_UNUSED, GLogLevelFlags log_level G_GNUC_UNUSED, const gchar *message, gpointer unused_data G_GNUC_UNUSED)
{
    g_fprintf(logfile, "%s", message);
    fflush(logfile);
}


static gboolean signal_handler (gpointer user_data)
{
    CdemuDaemon *self = (CdemuDaemon*)user_data;
    cdemu_daemon_stop_daemon(self);
    return G_SOURCE_CONTINUE;
}

static void setup_signal_trap ()
{
    if (g_unix_signal_add(SIGTERM, signal_handler, daemon_obj) <= 0) {
        g_warning(Q_("Failed to add signal handler for SIGTERM!"));
    }
    if (g_unix_signal_add(SIGINT, signal_handler, daemon_obj) <= 0) {
        g_warning(Q_("Failed to add signal handler for SIGINT!"));
    }
    /* SIGQUIT not supported by g_unix_signal_source_new: */
    if (g_unix_signal_add(SIGHUP, signal_handler, daemon_obj) <= 0) {
        g_warning(Q_("Failed to add signal handler for SIGHUP!"));
    }
}


/******************************************************************************\
 *                                Main function                               *
\******************************************************************************/
int main (int argc, char **argv)
{
    /* Localization support */
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    /* Default log handler is local */
    g_log_set_default_handler(log_handler_stdout, NULL);

    /* Glib's commandline parser */
    GError *error = NULL;
    GOptionContext *option_context;
    gboolean succeeded;

    option_context = g_option_context_new(NULL);
    g_option_context_set_translation_domain(option_context, GETTEXT_PACKAGE);
    g_option_context_add_main_entries(option_context, option_entries, GETTEXT_PACKAGE);
    succeeded = g_option_context_parse(option_context, &argc, &argv, &error);
    g_option_context_free(option_context);

    if (!succeeded) {
        g_warning(Q_("Failed to parse options: %s\n"), error->message);
        g_error_free(error);
        return -1;
    }

    /* Set up logfile handler, if necessary */
    if (log_filename) {
        logfile = fopen(log_filename, "w"); /* Overwrite log file */
        if (!logfile) {
            g_warning(Q_("Failed to open log file %s for writing!\n"), log_filename);
            return -1;
        }
        g_log_set_default_handler(log_handler_logfile, NULL);
    }

    /* Initialize libMirage */
    if (!mirage_initialize(&error)) {
        g_warning(Q_("Failed to initialize libMirage: %s!\n"), error->message);
        g_error_free(error);
        return -1;
    }

    /* Display status */
    g_message(Q_("Starting CDEmu daemon with following parameters:\n"));
    g_message(Q_(" - num devices: %i\n"), num_devices);
    g_message(Q_(" - control device: %s\n"), ctl_device);
    g_message(Q_(" - audio driver: %s\n"), audio_driver);
    g_message(Q_(" - bus type: %s\n"), bus);
    g_message(Q_(" - default CDEmu debug mask: 0x%X\n"), cdemu_debug_mask);
    g_message(Q_(" - default libMirage debug mask: 0x%X\n"), mirage_debug_mask);
    g_message("\n");

    /* Decipher bus type */
    gboolean use_system_bus = FALSE;
    if (!mirage_helper_strcasecmp(bus, "system")) {
        use_system_bus = TRUE;
    } else if (!mirage_helper_strcasecmp(bus, "session")) {
        use_system_bus = FALSE;
    } else {
        g_warning(Q_("Invalid bus argument '%s', using default bus!\n"), bus);
        use_system_bus = FALSE;
    }

    /* Discourage the use of system bus */
    if (use_system_bus) {
        g_message(Q_("WARNING: using CDEmu on system bus is deprecated and might lead to security issues on multi-user systems! Consult the README file for more details.\n\n"));
    }

    /* Create daemon */
    daemon_obj = g_object_new(CDEMU_TYPE_DAEMON, NULL);

    /* Signal trapping */
    setup_signal_trap();

    /* Initialize and start daemon */
    if (cdemu_daemon_initialize_and_start(daemon_obj, num_devices, ctl_device, audio_driver, use_system_bus, cdemu_debug_mask, mirage_debug_mask)) {
        /* Printed when daemon stops */
        g_message(Q_("Stopping daemon.\n"));
    } else {
        g_warning(Q_("Daemon initialization and start failed!\n"));
        succeeded = FALSE;
    }

    /* Release daemon object */
    g_object_unref(daemon_obj);

    /* Shutdown libMirage */
    mirage_shutdown(NULL);

    /* Close log file, if necessary */
    if (log_filename) {
        fclose(logfile);
    }

    return succeeded ? 0 : -1;
}
