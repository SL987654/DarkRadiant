#include "KillComponentEditor.h"
#include "../SpecifierType.h"
#include "../Component.h"

#include "string/convert.h"

#include "i18n.h"

#include <wx/spinctrl.h>
#include <wx/stattext.h>

namespace objectives
{

namespace ce
{

// Registration helper
KillComponentEditor::RegHelper KillComponentEditor::regHelper;

// Constructor
KillComponentEditor::KillComponentEditor(wxWindow* parent, Component& component) :
	ComponentEditorBase(parent),
	_component(&component),
	_targetCombo(new SpecifierEditCombo(_panel, getChangeCallback(), SpecifierType::SET_STANDARD_AI()))
{
	_amount = new wxSpinCtrl(_panel, wxID_ANY);
	_amount->SetValue(1);
	_amount->SetRange(0, 65535);
	_amount->Bind(wxEVT_SPINCTRL, [&] (wxSpinEvent& ev) { writeToComponent(); });

	wxStaticText* label = new wxStaticText(_panel, wxID_ANY, _("Kill target:"));
	label->SetFont(label->GetFont().Bold());

	_panel->GetSizer()->Add(label, 0, wxBOTTOM, 6);
	_panel->GetSizer()->Add(_targetCombo, 0, wxBOTTOM | wxEXPAND, 6);

	_panel->GetSizer()->Add(new wxStaticText(_panel, wxID_ANY, _("Amount:")), 0, wxBOTTOM, 6);
	_panel->GetSizer()->Add(_amount, 0, wxBOTTOM, 6);

    // Populate the SpecifierEditCombo with the first specifier
    _targetCombo->setSpecifier(
        component.getSpecifier(Specifier::FIRST_SPECIFIER)
    );

	// Initialise the spin button with the value from the first component argument
	_amount->SetValue(string::convert<int>(component.getArgument(0)));
}

// Write to component
void KillComponentEditor::writeToComponent() const {
    assert(_component);
    _component->setSpecifier(
        Specifier::FIRST_SPECIFIER, _targetCombo->getSpecifier()
    );

	_component->setArgument(0, string::to_string(_amount->GetValue()));
}

} // namespace ce

}
