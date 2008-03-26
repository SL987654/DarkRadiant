#include "Document.h"
#include "XPathException.h"

#include <libxml/xpath.h>

#include <iostream>

namespace xml
{

// Construct a wrapper around the provided xmlDocPtr.
Document::Document(xmlDocPtr doc):
    _xmlDoc(doc)
{}

Document::Document(const std::string& filename) :
	_xmlDoc(xmlParseFile(filename.c_str()))
{}

Document::Document(const Document& other) :
	_xmlDoc(other._xmlDoc) 
{}

Document::~Document() {
	if (_xmlDoc != NULL) {
		// Free the xml document memory
		xmlFreeDoc(_xmlDoc);
	}
}

Document Document::create() {
	// Create a new xmlDocPtr and return the object
	return Document(xmlNewDoc(xmlCharStrdup("1.0")));
}

void Document::addTopLevelNode(const std::string& name) {
	if (!isValid()) {
		return; // is not Valid, place an assertion here?
	}

	_xmlDoc->children = xmlNewDocNode(_xmlDoc, NULL, 
  									  xmlCharStrdup(name.c_str()), 
  									  xmlCharStrdup(""));
}

void Document::copyNodes(const NodeList& nodeList) {
	if (!isValid() || _xmlDoc->children == NULL) {
		return; // is not Valid, place an assertion here?
	}

	// Copy the child nodes one by one
	for (std::size_t i = 0; i < nodeList.size(); i++) {
		// Copy the node
		xmlNodePtr node = xmlCopyNode(nodeList[i].getNodePtr(), 1);
		// Add this node to the top level node of this document
		xmlAddChild(_xmlDoc->children, node);
	}
}

bool Document::isValid() const {
	return _xmlDoc != NULL;
}

// Evaluate an XPath expression and return matching Nodes.
NodeList Document::findXPath(const std::string& path) const {
    // Set up the XPath context
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;
    
    context = xmlXPathNewContext(_xmlDoc);
    if (context == NULL) {
        std::cerr << "ERROR: xml::findPath() failed to create XPath context "
                  << "when searching for " << path << std::endl;
        throw XPathException("Failed to create XPath context");
    }
    
    // Evaluate the expression  
    const xmlChar* xpath = reinterpret_cast<const xmlChar*>(path.c_str());
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);

    if (result == NULL) {
        std::cerr << "ERROR: xml::findPath() failed to evaluate expression "
                  << path << std::endl;
        throw XPathException("Failed to evaluate XPath expression");
    }
    
    // Construct the return vector. This may be empty if the provided XPath
    // expression does not identify any nodes.
    NodeList retval;
    xmlNodeSetPtr nodeset = result->nodesetval;
	if (nodeset != NULL) {
	    for (int i = 0; i < nodeset->nodeNr; i++) {
	        retval.push_back(Node(nodeset->nodeTab[i]));
	    }
	}

    xmlXPathFreeObject(result);
    return retval;
}

// Saves the file to the disk via xmlSaveFormatFile
void Document::saveToFile(const std::string& filename) {
	xmlSaveFormatFile(filename.c_str(), _xmlDoc, 1);
}

}
