/*
 *  display.c - code for this nifty display.
 *	part of galculator
 *  	(c) 2002-2004 Simon Floery (chimaira@users.sf.net)
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "galculator.h"
#include "display.h"
#include "general_functions.h"
#include "config_file.h"
#include "math_functions.h"
#include "calc_basic.h"
#include "ui.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <glade/glade.h>

static GtkTextView 	*view;
static GtkTextBuffer 	*buffer;
static int 		display_result_counter = 0;
static int		display_result_line = 0;

static char	*number_mod_labels[5] = {" DEC ", " HEX ", " OCT ", " BIN ", NULL}, 
		*angle_mod_labels[4] = {" DEG ", " RAD ", " GRAD ", NULL},
		*notation_mod_labels[4] = {" ALG ", " RPN ", " FORM ", NULL};	

/*
 * display.c mainly consists of two parts: first display setup code
 * and second display manipulation code.
 */



/* this code is taken from the GTK 2.0 tutorial: 
 *		http://www.gtk.org/tutorial
 */

gboolean on_textview_button_press_event (GtkWidget *widget,
						GdkEventButton *event,
						gpointer user_data)
{
	static 			GdkAtom targets_atom = GDK_NONE;
	int			x, y;
	GtkTextIter		start, end;
	char 			*selected_text;
	
	if (event->button == 1)	{
		gtk_widget_get_pointer (widget, &x, &y);
		gtk_text_view_get_iter_at_location (view, &start, x, y);
		/* we return if we are in the first line */
		if (gtk_text_iter_get_line (&start) != display_result_line+1) return FALSE;
		/* we return if its the end iterator */
		if (gtk_text_iter_is_end (&start) == TRUE) return FALSE;
		end = start;
		if (!gtk_text_iter_starts_word(&start)) gtk_text_iter_backward_word_start (&start);
		if (!gtk_text_iter_ends_word(&end)) gtk_text_iter_forward_word_end (&end);
		selected_text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
		/* in a rare case, we get two options as selected_text */
		if (strchr (selected_text, ' ') != NULL) return FALSE;
		/* rather a hack: last_arith is ignored as one char only gets selected as
			a word with spaces (because of iter_[back|for]ward_..). So we have to
			ignore the open brackets. */
		if (strlen (selected_text) <= 2) return FALSE;
	
		activate_menu_item (selected_text);
	}	
	else if (event->button == 2) {
		/* it's pasting selection time ...*/
		/* Get the atom corresponding to the string "STRING" */
		if (targets_atom == GDK_NONE) \
			targets_atom = gdk_atom_intern ("STRING", FALSE);
	
		/* And request the "STRING" target for the primary selection */
		gtk_selection_convert (widget, GDK_SELECTION_PRIMARY, targets_atom, \
			GDK_CURRENT_TIME);
	}		
	return FALSE;
}

/* this code is taken from the GTK 2.0 tutorial: 
 *		http://www.gtk.org/tutorial
 */

void on_textview_selection_received (GtkWidget *widget,
					GtkSelectionData *data,
					guint time,
					gpointer user_data)
{
	/* **** IMPORTANT **** Check to see if retrieval succeeded  */
	/* occurs if we just press the middle button with no active selection */
	if (data->length < 0) return;
	
	/* Make sure we got the data in the expected form */
	if (data->type != GDK_SELECTION_TYPE_STRING) return;
	
	display_result_feed (data->data);

	return;
}

/*
 * display_create_text_tags - creates a tag for the result and (in-)active modules
 */

void display_create_text_tags ()
{
	int	pixels=0;
	
	/* note: wrap MUST NOT be set to none in order to justify the text! */
	if (current_status.notation == CS_PAN) pixels = 5;

	gtk_text_buffer_create_tag (buffer, "result",
		"justification", GTK_JUSTIFY_RIGHT,
		"font", prefs.result_font,
		"foreground", prefs.result_color,
		"pixels-above-lines", pixels,
		"pixels-below-lines", pixels,
		NULL);
	
	gtk_text_buffer_create_tag (buffer, "active_module",
 		"font", prefs.mod_font,
		"foreground", prefs.act_mod_color,
		"wrap-mode", GTK_WRAP_NONE,
		NULL);
	
	gtk_text_buffer_create_tag (buffer, "inactive_module",
		"font", prefs.mod_font,
		"foreground", prefs.inact_mod_color,
		"wrap-mode", GTK_WRAP_NONE,
		NULL);
	
	gtk_text_buffer_create_tag (buffer, "stack", 
		"font", prefs.stack_font,
		"foreground", prefs.stack_color,
		"justification", GTK_JUSTIFY_LEFT, 
		NULL);
}

/*
 * display_init. Call this function before any other function of this file.
 */

void display_init (GtkWidget *a_parent_widget)
{
	GdkColor		color;
	int			char_width;
	PangoContext 		*pango_context;
	PangoFontMetrics 	*font_metrics;
	GtkTextTag		*tag;
	PangoTabArray		*tab_array;
	GtkTextTagTable		*tag_table;
	
	current_status.calc_entry_start_new = FALSE;
	view = (GtkTextView *) glade_xml_get_widget (main_window_xml, "textview");
	gdk_color_parse (prefs.bkg_color, &color);
	gtk_widget_modify_base ((GtkWidget *)view, GTK_STATE_NORMAL, &color);
	
	buffer = gtk_text_view_get_buffer (view);
	display_create_text_tags ();

	/* compute the approx char/digit width and create a tab stops */
	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, "active_module");
	/* get the approx char width */
	pango_context = gtk_widget_get_pango_context ((GtkWidget *)view);
	font_metrics = pango_context_get_metrics(pango_context, \
		tag->values->font,
		tag->values->language);
	char_width = MAX (pango_font_metrics_get_approximate_char_width (font_metrics), \
		pango_font_metrics_get_approximate_digit_width (font_metrics));
	tab_array = pango_tab_array_new_with_positions (1, FALSE, PANGO_TAB_LEFT, 3*char_width);
	gtk_text_view_set_tabs (view, tab_array);
	pango_tab_array_free (tab_array);
	
	display_set_line (CLEARED_DISPLAY, display_result_line, "result");
	/* DISPLAY RESULT MODIFIED
	gtk_text_buffer_get_iter_at_line (buffer, &iter, display_result_line);
	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, 
		CLEARED_DISPLAY, -1, "result", NULL);
	*/
	display_update_modules ();

	/* number, angle and notation are now set in src/callbacks.c::
	 * on_scientific_mode_activate resp on_basic_mode_activate.
	 */
}

/*
 * display_module_arith_label_update - code for the "display last arithmetic  
 * operation" display module. put operation in static current_char to survive
 * an apply f preferences.
 */

void display_module_arith_label_update (char operation)
{
	GtkTextMark	*this_mark;
	GtkTextIter	start, end;
	static char 	current_char=' ';
	
	if ((prefs.vis_arith == FALSE) || (prefs.mode != SCIENTIFIC_MODE)) 
		return;
	if (strchr ("()", operation) != NULL) return;
	
	if ((this_mark = gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_ARITH)) == NULL) \
		return;
	
	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
	end = start;
	gtk_text_iter_forward_chars (&end, 3);
	gtk_text_buffer_delete (buffer, &start, &end);
	
	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
	if (operation != NOP) current_char = operation;

	gtk_text_buffer_insert_with_tags_by_name (buffer, &start, \
		g_strdup_printf ("\t%c\t", current_char), -1, "active_module", NULL);
}

/*
 * display_module_bracket_label_update - code for the "display number of open
 * brackets" display module
 */

int display_module_bracket_label_update (int option)
{
	GtkTextMark	*this_mark;
	GtkTextIter	start, end;
	static int	nr_brackets=0;
	int		forward_count=2;
	char 		*string;
	
	switch (option) {
		case ONE_MORE:
			nr_brackets++;
			if (nr_brackets > 0) forward_count = 5 + log10(nr_brackets);
			break;
		case ONE_LESS:
			if (nr_brackets > 0) forward_count = 5 + log10(nr_brackets);
			nr_brackets--;
			break;
		case RESET:
			if (nr_brackets > 0) forward_count = 5 + log10(nr_brackets);
			nr_brackets = 0;
			break;
		case GET:
			/* doing this here to not touch the display */
			return nr_brackets;
		case NOP:
			if (nr_brackets > 0) forward_count = 5 + log10(nr_brackets);
			break;
	}
	if ((prefs.vis_bracket == FALSE) || (prefs.mode != SCIENTIFIC_MODE)) 
		return nr_brackets;
	
	if ((this_mark = gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_BRACKET)) == NULL) \
		return nr_brackets;
	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
	end = start;
	gtk_text_iter_forward_chars (&end, forward_count);
	gtk_text_buffer_delete (buffer, &start, &end);

	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);

	if (nr_brackets > 0) {
		string = g_strdup_printf ("\t(%i\t", nr_brackets);
		gtk_text_buffer_insert_with_tags_by_name (buffer, &start, string, \
			-1, "active_module", NULL);
		g_free (string);	
	}
	else {
		nr_brackets = 0;
		gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\t\t", \
			-1, "active_module", NULL);
	}
	return nr_brackets;
}

/*
 * display_module_base_create - start_mark is the mark where this module starts.
 * Therefore, the single labels are inserted starting with the last (as the mark
 * is at the beginning!). A simple gtk_text_buffer_insert_with_tags_by_name
 * doesn't work as we insert in an already tagged region an tag priority rules out.
 * solving this matter with setting priorities is not possible, so we have to remove
 * all tags and apply the tag we want.
 */

void display_module_base_create (char **module_label, char *mark_name, int active_index)
{
	int		label_counter=0, counter;
	GtkTextIter 	start, end;
	GtkTextMark	*this_mark;

	if ((this_mark = gtk_text_buffer_get_mark (buffer, mark_name)) == NULL) return;
	while (module_label[label_counter] != NULL) label_counter++;
	for (counter = (label_counter-1); counter >= 0; counter--) {
		gtk_text_buffer_get_iter_at_mark (buffer, &end, this_mark);
		gtk_text_buffer_insert (buffer, &end, module_label[counter], -1);
		gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
		gtk_text_buffer_remove_all_tags (buffer, &start, &end);
		if (counter == active_index) {
			gtk_text_buffer_apply_tag_by_name (buffer, "active_module", &start, &end);
		} else {
			gtk_text_buffer_apply_tag_by_name (buffer, "inactive_module", &start, &end);	
		}
	}
}

/*
 * display_module_leading_spaces - the distance between two modules is done with 
 * spaces. when inserting this spaces, we have to pay attention that it really 
 * is between two modules (e.g. we want no spaces after the last module) and 
 * that we don't lose the text mark pointing to the start of a module.
 */

void display_module_leading_spaces (char *mark_name, gboolean leading_spaces)
{	
	GtkTextIter	iter;
	GtkTextMark	*start_mark;
	
	if (leading_spaces == FALSE) return;
	
	if ((start_mark = gtk_text_buffer_get_mark (buffer, mark_name)) == NULL) return;
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, start_mark);
	/* insert spaces */
	gtk_text_buffer_insert (buffer, &iter, DISPLAY_MODULES_DELIM, -1);
	/* update text mark */
	gtk_text_buffer_delete_mark (buffer, start_mark);
	gtk_text_buffer_create_mark (buffer, mark_name, &iter, TRUE);
}

/* display_module_number_activate.
 */

void display_module_number_activate (int number_base)
{
	activate_menu_item (number_mod_labels[number_base]);
}

/* display_module_angle_activate.
 */

void display_module_angle_activate (int angle_unit)
{
	activate_menu_item (angle_mod_labels[angle_unit]);
}

/* display_module_notation_activate.
 */

void display_module_notation_activate (int mode)
{
	activate_menu_item (notation_mod_labels[mode]);
}

void display_get_line_end_iter (GtkTextBuffer *b, int line_index, GtkTextIter *end)
{
	gtk_text_buffer_get_iter_at_line (b, end, line_index);
	gtk_text_iter_forward_to_line_end (end);
}

void display_get_line_iters (GtkTextBuffer *b, int line_index, GtkTextIter *start, GtkTextIter *end)
{	
	gtk_text_buffer_get_iter_at_line (b, start, line_index);
	*end = *start;
	gtk_text_iter_forward_to_line_end (end);
}

/*
 * display_delete_line - deletes given line. start points to the place where we
 *	deleted.
 */

void display_delete_line (GtkTextBuffer *b, int line_index, GtkTextIter *iter)
{
	GtkTextIter start, end;
	
	if (gtk_text_buffer_get_line_count (b) <= line_index) return;
	display_get_line_iters (b, line_index, &start, &end);
	gtk_text_buffer_delete (buffer, &start, &end);
	*iter = start;
}

/*
 * display_create_modules - the display modules compose the second line of then
 *	display. available modules:
 *		- change number base
 *		- change angle base
 *		- change notation
 */

void display_update_modules ()
{
	GtkTextIter	start, end;
	gboolean	first_module = TRUE;
	
	
	display_get_line_end_iter (buffer, display_result_line, &start);
	display_get_line_end_iter (buffer, display_result_line+1, &end);
	gtk_text_buffer_delete (buffer, &start, &end);
	
	if (prefs.mode == BASIC_MODE) return;
	
	/* change number base */
	if (prefs.vis_number == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_NUMBER) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_NUMBER);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_NUMBER, &start, TRUE);
		display_module_base_create (number_mod_labels, DISPLAY_MARK_NUMBER, current_status.number);
		display_module_leading_spaces (DISPLAY_MARK_NUMBER, !first_module);
		first_module = FALSE;
	}
	
	/* angle */
	if (prefs.vis_angle == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_ANGLE) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_ANGLE);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_ANGLE, &start, TRUE);
		display_module_base_create (angle_mod_labels, DISPLAY_MARK_ANGLE, current_status.angle);
		display_module_leading_spaces (DISPLAY_MARK_ANGLE, !first_module);
		first_module = FALSE;
	}
	
	/* notation */
	if (prefs.vis_notation == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_NOTATION) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_NOTATION);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_NOTATION, &start, TRUE);
		display_module_base_create (notation_mod_labels, DISPLAY_MARK_NOTATION, current_status.notation);
		display_module_leading_spaces (DISPLAY_MARK_NOTATION, !first_module);
		first_module = FALSE;
	}	
	
	/* last arithmetic operation */
	if (prefs.vis_arith == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_ARITH) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_ARITH);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_ARITH, &start, TRUE);
		display_module_arith_label_update (NOP);
		display_module_leading_spaces (DISPLAY_MARK_ARITH, !first_module);
		first_module = FALSE;
	}
	
	/* number of open brackets */
	if (prefs.vis_bracket == TRUE) {
		if (first_module) 
			gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "\n", \
				-1, "result", NULL);
		gtk_text_buffer_get_iter_at_line (buffer, &start, display_result_line+1);
		gtk_text_iter_forward_to_line_end (&start);
		if (gtk_text_buffer_get_mark (buffer, DISPLAY_MARK_BRACKET) != NULL) \
			gtk_text_buffer_delete_mark_by_name (buffer, DISPLAY_MARK_BRACKET);
		gtk_text_buffer_create_mark (buffer, DISPLAY_MARK_BRACKET, &start, TRUE);
		display_module_bracket_label_update (NOP);
		display_module_leading_spaces (DISPLAY_MARK_BRACKET, !first_module);
		first_module = FALSE;
	}
}

/*
 * display_module_base_delete - delete the given module at given text mark.
 * used for display_change_option
 */

void display_module_base_delete (char *mark_name, char **text)
{
	int		counter=0, length=0;
	GtkTextIter	start, end;
	GtkTextMark	*this_mark;
	
	if ((this_mark = gtk_text_buffer_get_mark (buffer, mark_name)) == NULL) return;
		
	while (text[counter] != NULL) {
		length+=strlen(text[counter]);
		counter++;
	}

	gtk_text_buffer_get_iter_at_mark (buffer, &start, this_mark);
	end = start;
	gtk_text_iter_forward_chars (&end, length);
	gtk_text_buffer_delete (buffer, &start, &end);
}


/*
 * display_change_option - changes CURRENT_STATUS (!) and updates the display. 
 * The last function in the signal handling cascade of changing base etc.
 */

void display_change_option (int new_status, int opt_group)
{
	double	display_value=0;
	double 	*stack;
	
	switch (opt_group) {
		case DISPLAY_OPT_NUMBER:
			update_active_buttons (new_status, current_status.notation);
			if (current_status.number == new_status) return;
			display_value = display_result_get_double ();
			stack = display_stack_get_yzt_double ();
			current_status.number = new_status;
			display_result_set_double (display_value);
			display_stack_set_yzt_double (stack);
			g_free (stack);
			if ((prefs.vis_number) && (prefs.mode == SCIENTIFIC_MODE)) {
				display_module_base_delete (DISPLAY_MARK_NUMBER, number_mod_labels);
				display_module_base_create (number_mod_labels, DISPLAY_MARK_NUMBER, current_status.number);
			}
			break;
		case DISPLAY_OPT_ANGLE:
			if (current_status.angle == new_status) return;
			current_status.angle = new_status;
			if ((prefs.vis_angle) && (prefs.mode == SCIENTIFIC_MODE)){
				display_module_base_delete (DISPLAY_MARK_ANGLE, angle_mod_labels);
				display_module_base_create (angle_mod_labels, DISPLAY_MARK_ANGLE, current_status.angle);
			}
			break;
		case DISPLAY_OPT_NOTATION:
			update_active_buttons (current_status.number, new_status);
			if (current_status.notation == new_status) return;
			current_status.notation = new_status;
			if ((prefs.vis_notation) && (prefs.mode == SCIENTIFIC_MODE)){
				display_module_base_delete (DISPLAY_MARK_NOTATION, notation_mod_labels);
				display_module_base_create (notation_mod_labels, DISPLAY_MARK_NOTATION, current_status.notation);
			}
			break;
		default:
			fprintf (stderr, _("[%s] unknown display option in function \"display_change_option\". %s\n"), PROG_NAME, BUG_REPORT);
	}
	current_status.calc_entry_start_new = TRUE;
}

/*
 * display_set_bkg_color - change the background color of the text view
 */

void display_set_bkg_color (char *color_string)
{
	GdkColor	color;
	
	gdk_color_parse (color_string, &color);
	gtk_widget_modify_base ((GtkWidget *)view, GTK_STATE_NORMAL, &color);
}

/*
 * if we change any settings for result/module fonts this function updates the
 * all the tags.
 */

void display_update_tags ()
{
	GtkTextIter		start, end;
	GtkTextTagTable		*tag_table;
	GtkTextTag 		*tag;
	
	/* remove all tags from tag_table, so we can define the new tags */
	tag_table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (tag_table, "result");
	gtk_text_tag_table_remove (tag_table, tag);
	tag = gtk_text_tag_table_lookup (tag_table, "inactive_module");
	gtk_text_tag_table_remove (tag_table, tag);
	tag = gtk_text_tag_table_lookup (tag_table, "active_module");
	gtk_text_tag_table_remove (tag_table, tag);
	tag = gtk_text_tag_table_lookup (tag_table, "stack");
	gtk_text_tag_table_remove (tag_table, tag);
	
	/* create the tags again, up2date */
	display_create_text_tags ();
	
	/* apply to stack */
	if (display_result_line > 0) {
		gtk_text_buffer_get_iter_at_line (buffer, &start, 0);
		display_get_line_end_iter (buffer, display_result_line - 1, &end);
		gtk_text_buffer_apply_tag_by_name (buffer, "stack", &start, &end);
	}

	/* apply to result */
	display_get_line_iters (buffer, display_result_line, &start, &end);
	gtk_text_buffer_apply_tag_by_name (buffer, "result", &start, &end);
	
	/* apply to modules */
	display_update_modules ();
}

/******************
 *** END of display CONFIGuration code.
 *** from here on display manipulation code.
 ******************/

/*
 * display_result_add_digit. appends the given digit to the current entry, 
 * handles zeros, decimal points. call e.g. with *(gtk_button_get_label (button))
 */
void display_result_add_digit (char digit)
{
	char			*display_string=NULL, *new_display_string=NULL;

	if (current_status.calc_entry_start_new == TRUE) {
		/* fool the following code */
		display_result_set ("0");
		current_status.calc_entry_start_new = FALSE;
		display_result_counter = 1;
	}
	
	display_string = display_result_get();
	
	if (digit == dec_point[0]) {
		/* don't manipulate display_result_counter here! */
		if (strlen (display_string) == 0)
			new_display_string = g_strdup_printf ("0%c", digit);
		else if ((strchr (display_string, dec_point[0]) == NULL) && \
					(strchr (display_string, 'e') == NULL))
			new_display_string = g_strdup_printf ("%s%c", display_string, digit);
	} else {
		/* replace "0" on display with new digit */
		if (strcmp (display_string, "0") == 0) new_display_string = g_strdup_printf ("%c", digit);
		else if (display_result_counter < get_display_number_length(current_status.number)) {
			new_display_string = g_strdup_printf ("%s%c", display_string, digit);
			/* increment counter only in this if directive as above the counter remains 1! */
			display_result_counter++;
		}
	}
	if (new_display_string) display_result_set (new_display_string);
	if (display_string) g_free (display_string);
	if (new_display_string) g_free (new_display_string);
}

void display_stack_create ()
{
	int 		counter;
	GtkTextIter	start;
	
	if (display_result_line > 0) return;
	display_result_line = 3;
	for (counter = 0; counter < display_result_line; counter++) {
		gtk_text_buffer_get_iter_at_line (buffer, &start, 0);
		gtk_text_buffer_insert_with_tags_by_name (buffer, &start, "0\n",
			-1, "stack", NULL);
	}
}

void display_stack_remove ()
{
	GtkTextIter	start, end;
	
	gtk_text_buffer_get_iter_at_line (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_line (buffer, &end, display_result_line);
	gtk_text_buffer_delete (buffer, &start, &end);
	display_result_line = 0;
}

/* 
 * display_stack_set_yzt_double - stack must be a 3 dimensional array. stack in
 *	calc_basic IS NOT modified.
 */

void display_stack_set_yzt_double (double *stack)
{
	int		counter;

	for (counter = 0; counter < display_result_line; counter++)
		display_set_line_double (stack[counter], 
			display_result_line - counter - 1, "stack");
}

/* 
 * display_stack_set_yzt_double - stack in calc_basic IS NOT modified.
 */

void display_stack_set_yzt (char **stack)
{
	int		counter;
	
	for (counter = 0; counter < display_result_line; counter++) {
		display_set_line (stack[display_result_line - counter - 1], 
			counter, "stack");
	}
}

/*
 * display_stack_get_yzt - returns an three-sized char array with 1:1 copies
 *	of the current displayed stack.
 */

char **display_stack_get_yzt ()
{
	char 		**stack;
	int 		counter;
	
	stack = (char **) malloc (3* sizeof (char *));
	for (counter = 0; counter < 3; counter++) {
		/*  DISPLAY RESULT GET 
		display_get_line_iters (buffer, display_result_line - counter - 1, &start, &end);
		stack[counter] = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
		*/
		stack[counter] = display_get_line(display_result_line - counter - 1);
	}
	return stack;
}

/*
 * display_stack_get_yzt_double - returns the string array of display_stack_get_yzt
 * 	as double array.
 */

double *display_stack_get_yzt_double ()
{
	char 	**string_stack;
	double	*double_stack;
	int	counter;
	
	double_stack = (double *) malloc (display_result_line * sizeof(double));
	string_stack = display_stack_get_yzt();
	for (counter = 0; counter < display_result_line; counter++)
		double_stack[counter] = string2double (string_stack[counter], current_status.number);
	return double_stack;
}

void display_set_line_double (double value, int line, char *tag)
{
	char		*string_value;
	
	string_value = get_display_number_string (value, current_status.number);
	display_set_line (string_value, line, tag);
	g_free (string_value);
}

void display_set_line (char *string, int line, char *tag)
{
	char 		*separator_string;
	GtkTextIter	start;

	/* at first clear the result field */
	display_delete_line (buffer, line, &start);
	
	separator_string = string_add_separator(string, get_sep(current_status.number), 
		get_sep_length(current_status.number), get_sep_char(current_status.number), dec_point[0]);
	/* DISPLAY RESULT MODIFIED */
	gtk_text_buffer_insert_with_tags_by_name (buffer, &start, separator_string, -1, tag, NULL);
	free (separator_string);
	if (line == display_result_line) {
		display_result_counter = strlen (string);
		/* this is some cosmetics. try to keep counter up2date */
		if (strchr (string, 'e') != NULL) {
			display_result_counter -= (strchr(string, 'e') + sizeof(char) - string)/sizeof(char);
			display_result_counter += get_display_number_length(current_status.number) - DISPLAY_RESULT_E_LENGTH - 1;
		}
		else if (strchr (string, dec_point[0]) != NULL) display_result_counter--;
	}
}

/*
 * display_result_set_double. set text of calc_entry to string given by float value.
 *	the float value is manipulated (rounded, ...)
 */

void display_result_set_double (double value)
{	
	current_status.allow_arith_op = TRUE;
	display_module_arith_label_update (' ');
	display_set_line_double (value, display_result_line, "result");
}

void display_result_set (char *string_value)
{
	current_status.allow_arith_op = TRUE;
	display_module_arith_label_update (' ');
	display_set_line (string_value, display_result_line, "result");
}

void display_result_feed (char *string)
{
	int	counter;
	
	for (counter = 0; counter < strlen(string); counter++) {
		if (is_valid_number(current_status.number, string[counter])) 
			/* g_ascii_toupper to convert lower hex chars to upper */
			display_result_add_digit (
				g_ascii_toupper(string[counter]));
	}
	if (string[0] == '-') display_result_toggle_sign (NULL);
}

/*
 * display_result_get_line. The only function calling a gtk_text_buffer_get_*
 *	function directly.
 */

char *display_get_line (int line_nr)
{
	GtkTextIter 	start, end;
	
	display_get_line_iters (buffer, line_nr, &start, &end);
	/* DISPLAY RESULT GET */
	return string_del_separator(gtk_text_buffer_get_text (buffer, &start, &end, TRUE), get_sep_char(current_status.number));
}

/* display_result_get. a simplfied call to display_result_get_line
 */

char *display_result_get ()
{
	return display_get_line (display_result_line);
}

/* display_result_get_double. a display_result_get plus conversion to double
 */

double display_result_get_double ()
{
	char 	*result_string;
	double	ret_val;
	
	result_string = display_result_get();
	ret_val = string2double (result_string, current_status.number);
	g_free (result_string);
		
	return ret_val;
}	

void display_append_e (GtkToggleButton *button)
{
	char			*display_value;

	/* we have kind of a shortcut. we don't set to 0e+ but 1e+ */
	if (current_status.number != CS_DEC) return;
	if ((current_status.calc_entry_start_new == FALSE) && 
		(strcmp (display_result_get(), "0") != 0)) {
		if (strstr (display_result_get(), "e") == NULL) {
			/* DISPLAY RESULT MODIFIED
			display_get_line_end_iter (buffer, display_result_line, &end);
			gtk_text_buffer_insert_with_tags_by_name (buffer, &end, "e+", -1, "result", NULL);
			*/
			display_value = g_strdup_printf ("%se+", display_result_get());
			display_result_set (display_value);
			g_free (display_value);
		}
	} else {
		display_result_set ("1e+");
		current_status.calc_entry_start_new = FALSE;
	}
	display_result_counter = get_display_number_length(current_status.number) - DISPLAY_RESULT_E_LENGTH;
}

void display_result_toggle_sign (GtkToggleButton *button)
{
	char			*result_field, *e_pointer;

	if (current_status.number != CS_DEC) return;
	result_field = display_result_get();
	/* if there is no e? we toggle the leading sign, otherwise the sign after e */
	if ((e_pointer = strchr (result_field, 'e')) == NULL) {
		if (*result_field == '-') g_stpcpy (result_field, result_field + sizeof(char));
		else if (strcmp (result_field, "0") != 0)
			result_field = g_strdup_printf ("-%s", result_field);
	} else {
		if (*(++e_pointer) == '-') *e_pointer = '+';
		else *e_pointer = '-';
	}
	display_result_set (result_field);
	g_free (result_field);
}

/* display_result_backspace - deletes the tail of the display.
 *		sets the display with display_result_set => no additional manipulation of 
 *		display_result_counter
 *		necessary
 */

void display_result_backspace ()
{															
	char	*current_entry;

	if (current_status.calc_entry_start_new == TRUE) {
		current_status.calc_entry_start_new = FALSE;
		display_result_set ("0");
	} else {
		current_entry = display_result_get();
		/* to avoid an empty/senseless result field */
		if (strlen(current_entry) == 1) current_entry[0] = '0';
		else if ((strlen(current_entry) == 2) && (*current_entry == '-')) current_entry = "0\0";
		else if (current_entry[strlen(current_entry) - 2] == 'e') current_entry[strlen(current_entry) - 2] = '\0';
		else current_entry[strlen(current_entry) - 1] = '\0';
		display_result_set (current_entry);
		g_free (current_entry);
	}
}

/* display_result_getset. kind of display result redraw. use this to get and
 * set the display, it frees the return string of display_result_get()
 */

void display_result_getset ()
{
	char	*result;
	char	**stack;
	
	result = display_result_get();
	stack = display_stack_get_yzt();
	display_result_set(result);
	display_stack_set_yzt(stack);
	g_free(result);
	g_free(stack);
}
