#include "FindShader.h"

#include "mainframe.h"
#include "gtkutil/LeftAlignedLabel.h"
#include "gtkutil/LeftAlignment.h"
#include "gtkutil/IconTextButton.h"
#include "ui/common/ShaderChooser.h"
#include <gtk/gtk.h>

namespace ui {

	namespace {
		const int FINDDLG_DEFAULT_SIZE_X = 550;
	    const int FINDDLG_DEFAULT_SIZE_Y = 100;
	   	
	   	const char* LABEL_FINDSHADER = "Find and Replace Shader";
	   	const char* LABEL_FIND = "Find:";
	   	const char* LABEL_REPLACE = "Replace:";
	   	
	   	const char* FOLDER_ICON = "folder16.png";
	   	
	    const std::string FINDDLG_WINDOW_TITLE = "Find & Replace Shader";
	}

FindAndReplaceShader::FindAndReplaceShader() :
	DialogWindow(FINDDLG_WINDOW_TITLE, MainFrame_getWindow())
{
	setWindowSize(FINDDLG_DEFAULT_SIZE_X, FINDDLG_DEFAULT_SIZE_Y);
	gtk_container_set_border_width(GTK_CONTAINER(_window), 12);
	gtk_window_set_type_hint(GTK_WINDOW(_window), GDK_WINDOW_TYPE_HINT_DIALOG);
	
	// Create all the widgets
	populateWindow();
	
	// Show the window and its children
	gtk_widget_show_all(_window);
}

void FindAndReplaceShader::populateWindow() {
	GtkWidget* dialogVBox = gtk_vbox_new(false, 6);
	gtk_container_add(GTK_CONTAINER(_window), dialogVBox);
	
	// Create the title label (bold font)
	GtkWidget* topLabel = gtkutil::LeftAlignedLabel(
    	std::string("<span weight=\"bold\">") + LABEL_FINDSHADER + "</span>"
    );
    gtk_box_pack_start(GTK_BOX(dialogVBox), topLabel, true, true, 0);
    
    GtkWidget* spacer2 = gtk_alignment_new(0,0,0,0);
	gtk_widget_set_usize(spacer2, 10, 1);
	gtk_box_pack_start(GTK_BOX(dialogVBox), spacer2, false, false, 0);
    
    GtkWidget* findHBox = gtk_hbox_new(false, 0);
    GtkWidget* replaceHBox = gtk_hbox_new(false, 0);
    
    // Pack these hboxes into an alignment so that they are indented
	GtkWidget* alignment = gtkutil::LeftAlignment(GTK_WIDGET(findHBox), 18, 1.0); 
	GtkWidget* alignment2 = gtkutil::LeftAlignment(GTK_WIDGET(replaceHBox), 18, 1.0);
		
	gtk_box_pack_start(GTK_BOX(dialogVBox), GTK_WIDGET(alignment), true, true, 0); 
	gtk_box_pack_start(GTK_BOX(dialogVBox), GTK_WIDGET(alignment2), true, true, 0);
	
	// Create the labels and pack them in the hbox
	GtkWidget* findLabel = gtkutil::LeftAlignedLabel(LABEL_FIND);
	GtkWidget* replaceLabel = gtkutil::LeftAlignedLabel(LABEL_REPLACE);
	gtk_widget_set_size_request(findLabel, 60, -1);
	gtk_widget_set_size_request(replaceLabel, 60, -1);
	
	gtk_box_pack_start(GTK_BOX(findHBox), findLabel, false, false, 0);
	gtk_box_pack_start(GTK_BOX(replaceHBox), replaceLabel, false, false, 0);
	
	_findEntry = gtk_entry_new();
	_replaceEntry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(findHBox), _findEntry, true, true, 6);
	gtk_box_pack_start(GTK_BOX(replaceHBox), _replaceEntry, true, true, 6);
		
	// Create the icon buttons to open the ShaderChooser and override the size request
	_findSelectButton = gtkutil::IconTextButton("", FOLDER_ICON, false);
	gtk_widget_set_size_request(_findSelectButton, -1, -1); 
	g_signal_connect(G_OBJECT(_findSelectButton), "clicked", G_CALLBACK(callbackChooseFind), this);
		
	_replaceSelectButton = gtkutil::IconTextButton("", FOLDER_ICON, false);
	gtk_widget_set_size_request(_replaceSelectButton, -1, -1); 
	g_signal_connect(G_OBJECT(_replaceSelectButton), "clicked", G_CALLBACK(callbackChooseReplace), this);
	
	gtk_box_pack_start(GTK_BOX(findHBox), _findSelectButton, false, false, 0);
	gtk_box_pack_start(GTK_BOX(replaceHBox), _replaceSelectButton, false, false, 0);
	
	GtkWidget* spacer = gtk_alignment_new(0,0,0,0);
	gtk_widget_set_usize(spacer, 10, 2);
	gtk_box_pack_start(GTK_BOX(dialogVBox), spacer, false, false, 0);
	
	// Finally, add the buttons
	gtk_box_pack_start(GTK_BOX(dialogVBox), createButtons(), false, false, 0);
}

GtkWidget* FindAndReplaceShader::createButtons() {
	GtkWidget* hbox = gtk_hbox_new(false, 6);
	
	GtkWidget* replaceButton = gtk_button_new_from_stock(GTK_STOCK_FIND_AND_REPLACE);
	GtkWidget* closeButton = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	
	g_signal_connect(G_OBJECT(replaceButton), "clicked", G_CALLBACK(callbackReplace), this);
	g_signal_connect(G_OBJECT(closeButton), "clicked", G_CALLBACK(callbackClose), this);

	gtk_box_pack_end(GTK_BOX(hbox), closeButton, false, false, 0);
	gtk_box_pack_end(GTK_BOX(hbox), replaceButton, false, false, 0);
	return hbox;
}

void FindAndReplaceShader::performReplace() {
	
}

void FindAndReplaceShader::callbackChooseFind(GtkWidget* widget, FindAndReplaceShader* self) {
	// Construct the modal dialog, self-destructs on close
	new ShaderChooser(NULL, self->_window, self->_findEntry);
}

void FindAndReplaceShader::callbackChooseReplace(GtkWidget* widget, FindAndReplaceShader* self) {
	// Construct the modal dialog, self-destructs on close
	new ShaderChooser(NULL, self->_window, self->_replaceEntry);
}

void FindAndReplaceShader::callbackReplace(GtkWidget* widget, FindAndReplaceShader* self) {
	self->performReplace();
}

void FindAndReplaceShader::callbackClose(GtkWidget* widget, FindAndReplaceShader* self) {
	// Call the DialogWindow::destroy method and remove self from heap
	self->destroy();
}

void FindAndReplaceShader::show() {
	new FindAndReplaceShader(); // self-destructs in GTK callback
}

} // namespace ui
