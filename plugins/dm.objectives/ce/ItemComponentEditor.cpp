#include "ItemComponentEditor.h"
#include "../SpecifierType.h"
#include "../Component.h"

#include "i18n.h"
#include "string/convert.h"

#include <wx/stattext.h>
#include <wx/spinctrl.h>

namespace objectives 
{

namespace ce 
{

// Registration helper, will register this editor in the factory
ItemComponentEditor::RegHelper ItemComponentEditor::regHelper;

// Constructor
ItemComponentEditor::ItemComponentEditor(wxWindow* parent, Component& component) :
	ComponentEditorBase(parent),
	_component(&component),
	_itemSpec(new SpecifierEditCombo(_panel, getChangeCallback(), SpecifierType::SET_ITEM()))
{
	_amount = new wxSpinCtrl(_panel, wxID_ANY);
	_amount->SetValue(1);
	_amount->SetRange(0, 65535);
	_amount->Bind(wxEVT_SPINCTRL, [&] (wxSpinEvent& ev) { writeToComponent(); });

	wxStaticText* label = new wxStaticText(_panel, wxID_ANY, _("Item:"));
	label->SetFont(label->GetFont().Bold());

	_panel->GetSizer()->Add(label, 0, wxBOTTOM, 6);
	_panel->GetSizer()->Add(_itemSpec, 0, wxBOTTOM | wxEXPAND, 6);

	_panel->GetSizer()->Add(new wxStaticText(_panel, wxID_ANY, _("Amount:")), 0, wxBOTTOM, 6);
	_panel->GetSizer()->Add(_amount, 0, wxBOTTOM, 6);

    // Populate the SpecifierEditCombo with the first specifier
    _itemSpec->setSpecifier(
        component.getSpecifier(Specifier::FIRST_SPECIFIER)
    );

	// Initialise the spin button with the value from the first component argument
	_amount->SetValue(string::convert<int>(component.getArgument(0)));
}

// Write to component
void ItemComponentEditor::writeToComponent() const
{
    assert(_component);
    _component->setSpecifier(
        Specifier::FIRST_SPECIFIER, _itemSpec->getSpecifier()
    );

	_component->setArgument(0, string::to_string(_amount->GetValue()));
}

} // namespace ce

} // namespace objectives
