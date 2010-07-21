#ifndef SOUNDSHADERPREVIEW_H_
#define SOUNDSHADERPREVIEW_H_

#include <string>
#include <boost/shared_ptr.hpp>
#include <gtkmm/liststore.h>
#include <gtkmm/treeselection.h>

namespace Gtk
{ 
	class Widget;
	class HBox;
	class TreeView;
	class Button;
	class Label;
}

namespace ui
{

/** 
 * greebo: This class provides the UI elements to inspect a given
 * sound shader with playback option.
 * 
 * Use the GtkWidget* cast operator to pack this into a parent container. 
 */
class SoundShaderPreview
{
private:
	// The main container widget of this preview
	boost::shared_ptr<Gtk::HBox> _widget;
	
	// Tree store and view for available sound files, and the tree selection
	Glib::RefPtr<Gtk::ListStore> _listStore;

	Gtk::TreeView* _treeView;
	Glib::RefPtr<Gtk::TreeSelection> _selection;
	
	Gtk::Button* _playButton;
	Gtk::Button* _stopButton;
	Gtk::Label* _statusLabel;
	
	// The currently "previewed" soundshader
	std::string _soundShader;

	// Treemodel definition
	struct SoundListColumns : 
		public Gtk::TreeModel::ColumnRecord
	{
		SoundListColumns() { add(shader); }

		Gtk::TreeModelColumn<Glib::ustring> shader; // soundshader name
	};

	SoundListColumns _columns;
	
public:
	SoundShaderPreview();

	/** greebo: Sets the soundshader to preview. 
	 * 			This updates the preview liststore and treeview. 
	 */
	void setSoundShader(const std::string& soundShader);

	/** greebo: Operator cast to GtkWidget to pack this into a 
	 * 			parent container widget.
	 */
	operator GtkWidget*();

private:
	/** greebo: Returns the currently selected sound file (file list)
	 * 
	 * @returns: the filename as defined in the shader or "" if nothing selected.
	 */
	std::string getSelectedSoundFile();

	/** greebo: Creates the control widgets (play button) and such.
	 */
	Gtk::Widget* createControlPanel();

	/** greebo: Updates the list according to the active soundshader
	 */
	void update();
	
	// GTKmm Callbacks
	void onPlay();
	void onStop();
	void onSelectionChanged();
};

} // namespace ui

#endif /*SOUNDSHADERPREVIEW_H_*/
