/*
 *  ui.h - general user interface code.
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

extern GladeXML 	*main_window_xml;
extern GladeXML 	*button_box_xml;
extern GladeXML 	*prefs_xml;
extern GladeXML 	*about_dialog_xml;
extern GladeXML		*dispctrl_xml;
extern GtkListStore	*store;
extern char		dec_point[2];

/* active_buttons flags */
#define AB_DEC 1 << CS_DEC
#define AB_HEX 1 << CS_HEX
#define AB_OCT 1 << CS_OCT
#define AB_BIN 1 << CS_BIN

typedef struct {
	char		*button_name;
	unsigned int	mask;
} s_active_buttons;

typedef struct {
	const gchar 	*detailed_signal;
	GCallback	callback;
} s_signal_cb;
	

GtkWidget *ui_main_window_create ();
void ui_main_window_set_dispctrl (int location);
void ui_main_window_buttons_destroy ();
void ui_main_window_buttons_create (int mode);
void set_all_buttons_callback (gpointer *data);
void set_all_buttons_size (int width, int height);
void set_all_buttons_font (char *font_string);
void update_active_buttons (int number_base, int notation_mode);
void update_dispctrl ();
void button_activation (GtkToggleButton *b);
gboolean button_deactivation (gpointer data);
GtkWidget *ui_pref_dialog_create ();
GtkWidget *ui_about_dialog_create();
GtkWidget *ui_font_dialog_create (char *title, GtkButton *button);
GtkWidget *ui_color_dialog_create (char *title, GtkButton *button);
GtkWidget *ui_constants_menu_create (s_constant *constant, 
				GCallback const_handler);
GtkWidget *ui_memory_menu_create (s_array memory, 
				GCallback const_handler, 
				char *last_item);
GtkWidget *ui_right_mouse_menu_create ();
void ui_formula_entry_set (G_CONST_RETURN gchar *text);
void ui_formula_entry_insert (G_CONST_RETURN gchar *text);
void ui_formula_entry_backspace ();
void ui_button_set_pan ();
void ui_button_set_rpn ();
void position_menu (GtkMenu *menu, 
		gint *x, 
		gint *y, 
		gboolean *push_in, 
		gpointer user_data);
void set_widget_visibility (GladeXML *xml, char *widget_name, gboolean visible);
