/*
 *  main.c
 *	part of galculator
 *  	(c) 2002-2003 Simon Floery (chimaira@users.sf.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
/*
 * Initial main.c file generated by Glade. Edit as required.
 * Glade will not overwrite this file.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc_basic.h"
#include "galculator.h"
#include "display.h"
#include "config_file.h"
#include "general_functions.h"
#include "ui.h"

/* i18n */

#include <libintl.h>
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>

#define MASK_NUMLOCK GDK_MOD2_MASK

s_preferences		prefs;
s_current_status 	current_status = {0, 0, 0, 0, FALSE, FALSE, TRUE};
s_array			memory;
s_constant 		*constant;

void print_usage ()
{
	printf (_("\n%s v%s, (c) 2002-2003 Simon Floery\n\n\
Usage: %s [options]\n\n\
options:\n\
(GTK options)\n\
 -h, --help\t\tShow this usage message\n\
 -v, --version\t\tShow version information\n\n"), \
PACKAGE, VERSION, PACKAGE);
}

int key_snooper (GtkWidget *grab_widget, GdkEventKey *event, gpointer func_data)
{
	/* the problem: key acceleration in gtk2 works a bit strange. I do not
	 * understand it completely. The following is in part the result of a
	 * long trial and error process. If you can explain why it works, please
	 * tell me.
	 * btw, do this only if main_window is the current window. otherwise, 
	 * keypad's 0,2,4,6,8 won't work in gtkentry etc. (e.g. found in prefs)
	 */
	
	/*fprintf (stderr, "[%s] key snooper (1): %i %i %s\n", PROG_NAME, event->state, event->keyval, gdk_keyval_name (event->keyval));*/
	if (((event->keyval != GDK_KP_2) && (event->keyval != GDK_KP_Down) &&
		(event->keyval != GDK_KP_4) && (event->keyval != GDK_KP_Left) &&
		(event->keyval != GDK_KP_6) && (event->keyval != GDK_KP_Right) &&
		(event->keyval != GDK_KP_8) && (event->keyval != GDK_KP_Up) &&
		(event->keyval != GDK_KP_0) && (event->keyval != GDK_KP_Insert)) ||
		(strcmp (gtk_widget_get_name (gtk_widget_get_toplevel(grab_widget)), 
			"main_window") != 0))
			event->state &= ~GDK_MOD2_MASK;
	return FALSE;
}
	
int main (int argc, char *argv[])
{
	char		*config_file_name;
	GtkWidget 	*main_window;
	
	/*
	 * gtk_init runs (among other things) setlocale (LC_ALL, ""). Therefore we
	 * have to/can deal with i18n from this point.
	 * call gtk_init before getopts to get gtk options stripped (--display etc)
	 */

	gtk_init (&argc, &argv);
	
	bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);
    	
	if (argc > 1) {
		print_usage ();
		return EXIT_SUCCESS;
	}

	/* at first, get config file */
	config_file_name = g_strdup_printf ("%s/%s", getenv ("HOME"), CONFIG_FILE_NAME);
	prefs = config_file_read (config_file_name);

	constant = config_file_get_constants();
	g_free (config_file_name);

	/* at first get the main frame */
	
	main_window = ui_main_window_create();
	gtk_window_set_title ((GtkWindow *)main_window, PACKAGE);

	ui_main_window_buttons_create(prefs.mode);
	
	/* usually, only Shift, CTRL and ALT modifiers are paid attention to by 
	 * accelerator code. add MOD2 (NUMLOCK allover the worl?) to the list. 
	 * We have to do this for a working keypad.
	 */
	  
	gtk_accelerator_set_default_mod_mask (gtk_accelerator_get_default_mod_mask () | GDK_MOD2_MASK); 
				  
	/* prepare calc_basic */

	alg_init (0);
	rpn_init (prefs.stack_size, 0);

	/* finally show what we put together. do this as late asap */
	gtk_widget_show (main_window);

	/* main_windows has to be visible to get a nice and proper display */	
	display_init (main_window);
	
	/* apply changes */
	apply_preferences (prefs);

	remember_display_values ();

	memory.data = NULL;
	memory.len = 0;

	/* see function key_snooper for details */
	gtk_key_snooper_install (key_snooper, NULL);
	
	gtk_window_resize ((GtkWindow *)main_window, 1, 1);
	
	gtk_main ();

	/* save changes to file */

	config_file_name = g_strdup_printf ("%s/%s", getenv ("HOME"), CONFIG_FILE_NAME);
	config_file_write (config_file_name, prefs);
	g_free (config_file_name);

	return EXIT_SUCCESS;
}
