#include "GuiInstanceRepresentation.h"
#include "GuiInstanceLoader.h"
#include "InstanceQuery/GuiInstanceQuery.h"
#include "../Resources/GuiParserManager.h"
#include "../Controls/Templates/GuiControlTemplateStyles.h"

namespace vl
{
	namespace presentation
	{
		using namespace collections;
		using namespace parsing;
		using namespace parsing::xml;
		using namespace templates;
		using namespace stream;

/***********************************************************************
GuiTextRepr
***********************************************************************/

		Ptr<GuiValueRepr> GuiTextRepr::Clone()
		{
			auto repr = MakePtr<GuiTextRepr>();
			repr->fromStyle = fromStyle;
			repr->text = text;
			return repr;
		}

		void GuiTextRepr::FillXml(Ptr<parsing::xml::XmlElement> xml)
		{
			if (!fromStyle)
			{
				auto xmlText = MakePtr<XmlText>();
				xmlText->content.value = text;
				xml->subNodes.Add(xmlText);
			}
		}

/***********************************************************************
GuiAttSetterRepr
***********************************************************************/

		void GuiAttSetterRepr::CloneBody(Ptr<GuiAttSetterRepr> repr)
		{
			CopyFrom(repr->eventHandlers, eventHandlers);
			FOREACH_INDEXER(GlobalStringKey, name, index, setters.Keys())
			{
				Ptr<SetterValue> src = setters.Values()[index];
				Ptr<SetterValue> dst = new SetterValue;
				dst->binding = src->binding;
				FOREACH(Ptr<GuiValueRepr>, value, src->values)
				{
					dst->values.Add(value->Clone());
				}
				repr->setters.Add(name, dst);
			}
			repr->instanceName = instanceName;
		}

		Ptr<GuiValueRepr> GuiAttSetterRepr::Clone()
		{
			auto repr = MakePtr<GuiAttSetterRepr>();
			repr->fromStyle = fromStyle;
			CloneBody(repr);
			return repr;
		}

		void GuiAttSetterRepr::FillXml(Ptr<parsing::xml::XmlElement> xml)
		{
			if (!fromStyle)
			{
				if (instanceName != GlobalStringKey::Empty)
				{
					auto attName = MakePtr<XmlAttribute>();
					attName->name.value = L"ref.Name";
					attName->value.value = instanceName.ToString();
					xml->attributes.Add(attName);
				}

				for (vint i = 0; i < setters.Count(); i++)
				{
					auto key = setters.Keys()[i];
					auto value = setters.Values()[i];
					if (key == GlobalStringKey::Empty)
					{
						FOREACH(Ptr<GuiValueRepr>, repr, value->values)
						{
							repr->FillXml(xml);
						}
					}
					else
					{
						bool containsElement = false;
						FOREACH(Ptr<GuiValueRepr>, repr, value->values)
						{
							if (!repr.Cast<GuiTextRepr>())
							{
								containsElement = true;
								break;
							}
						}

						if (containsElement)
						{
							auto xmlProp = MakePtr<XmlElement>();
							xmlProp->name.value = L"att." + key.ToString();
							if (value->binding != GlobalStringKey::Empty)
							{
								xmlProp->name.value += L"-" + value->binding.ToString();
							}

							FOREACH(Ptr<GuiValueRepr>, repr, value->values)
							{
								if (!repr.Cast<GuiTextRepr>())
								{
									repr->FillXml(xmlProp);
								}
							}
							xml->subNodes.Add(xmlProp);
						}
						else if (value->values.Count() > 0)
						{
							auto att = MakePtr<XmlAttribute>();
							att->name.value = key.ToString();
							if (value->binding != GlobalStringKey::Empty)
							{
								att->name.value += L"-" + value->binding.ToString();
							}
							att->value.value = value->values[0].Cast<GuiTextRepr>()->text;
							xml->attributes.Add(att);
						}
					}
				}

				for (vint i = 0; i < eventHandlers.Count(); i++)
				{
					auto key = eventHandlers.Keys()[i];
					auto value = eventHandlers.Values()[i];

					auto xmlEvent = MakePtr<XmlElement>();
					xmlEvent->name.value = L"ev." + key.ToString();
					if (value->binding != GlobalStringKey::Empty)
					{
						xmlEvent->name.value += L"-" + value->binding.ToString();
					}
					xml->subNodes.Add(xmlEvent);

					auto xmlText = MakePtr<XmlCData>();
					xmlText->content.value = value->value;
					xmlEvent->subNodes.Add(xmlText);
				}

				for (vint i = 0; i < environmentVariables.Count(); i++)
				{
					auto key = environmentVariables.Keys()[i];
					auto value = environmentVariables.Values()[i];

					auto xmlEnvVar = MakePtr<XmlElement>();
					xmlEnvVar->name.value = L"env." + key.ToString();
					xml->subNodes.Add(xmlEnvVar);

					auto xmlText = MakePtr<XmlText>();
					xmlText->content.value = value->value;
					xmlEnvVar->subNodes.Add(xmlText);
				}
			}
		}

/***********************************************************************
GuiConstructorRepr
***********************************************************************/

		Ptr<GuiValueRepr> GuiConstructorRepr::Clone()
		{
			auto repr = MakePtr<GuiConstructorRepr>();
			repr->fromStyle = fromStyle;
			repr->typeNamespace = typeNamespace;
			repr->typeName = typeName;
			repr->styleName = styleName;
			CloneBody(repr);
			return repr;
		}

		void GuiConstructorRepr::FillXml(Ptr<parsing::xml::XmlElement> xml)
		{
			if (!fromStyle)
			{
				auto xmlCtor = MakePtr<XmlElement>();
				if (typeNamespace == GlobalStringKey::Empty)
				{
					xmlCtor->name.value = typeName.ToString();
				}
				else
				{
					xmlCtor->name.value = typeNamespace.ToString() + L":" + typeName.ToString();
				}

				if (styleName)
				{
					auto attStyle = MakePtr<XmlAttribute>();
					attStyle->name.value = L"ref.Style";
					attStyle->value.value = styleName.Value();
					xml->attributes.Add(attStyle);
				}

				GuiAttSetterRepr::FillXml(xmlCtor);
				xml->subNodes.Add(xmlCtor);
			}
		}

/***********************************************************************
GuiInstanceContext
***********************************************************************/

		void GuiInstanceContext::CollectDefaultAttributes(Ptr<GuiResourceItem> resource, GuiAttSetterRepr::ValueList& values, Ptr<parsing::xml::XmlElement> xml, GuiResourceError::List& errors)
		{
			if (auto parser = GetParserManager()->GetParser<ElementName>(L"INSTANCE-ELEMENT-NAME"))
			{
				// test if there is only one text value in the xml
				if (xml->subNodes.Count() == 1)
				{
					if (Ptr<XmlText> text = xml->subNodes[0].Cast<XmlText>())
					{
						Ptr<GuiTextRepr> value = new GuiTextRepr;
						value->text = text->content.value;
						value->tagPosition = text->content.codeRange.start;
						values.Add(value);
					}
					else if (Ptr<XmlCData> text = xml->subNodes[0].Cast<XmlCData>())
					{
						Ptr<GuiTextRepr> value = new GuiTextRepr;
						value->text = text->content.value;
						value->tagPosition = text->content.codeRange.start;
						value->tagPosition.column += 9; // <![CDATA[
						values.Add(value);
					}
				}

				// collect default attributes
				FOREACH(Ptr<XmlElement>, element, XmlGetElements(xml))
				{
					List<Ptr<ParsingError>> parsingErrors;
					auto name = parser->TypedParse(element->name.value, parsingErrors);
					GuiResourceError::Transform(resource, errors, parsingErrors, element->name.codeRange.start);
					if (name)
					{
						if (name->IsCtorName())
						{
							// collect constructor values in the default attribute setter
							auto ctor = LoadCtor(resource, element, errors);
							if (ctor)
							{
								values.Add(ctor);
							}
						}
						else if (!name->IsPropertyElementName() && !name->IsEventElementName())
						{
							errors.Add(GuiResourceError(resource, element->name.codeRange.start, L"Unknown element name: \"" + element->name.value + L"\"."));
						}
					}
				}
			}
		}

		void GuiInstanceContext::CollectAttributes(Ptr<GuiResourceItem> resource, GuiAttSetterRepr::SetteValuerMap& setters, Ptr<parsing::xml::XmlElement> xml, GuiResourceError::List& errors)
		{
			if (auto parser = GetParserManager()->GetParser<ElementName>(L"INSTANCE-ELEMENT-NAME"))
			{
				Ptr<GuiAttSetterRepr::SetterValue> defaultValue = new GuiAttSetterRepr::SetterValue;

				// collect default attributes
				CollectDefaultAttributes(resource, defaultValue->values, xml, errors);
				if (defaultValue->values.Count() > 0)
				{
					setters.Add(GlobalStringKey::Empty, defaultValue);
				}

				// collect values
				FOREACH(Ptr<XmlElement>, element, XmlGetElements(xml))
				{
					List<Ptr<ParsingError>> parsingErrors;
					auto name = parser->TypedParse(element->name.value, parsingErrors);
					GuiResourceError::Transform(resource, errors, parsingErrors, element->name.codeRange.start);
					if (name)
					{
						if (name->IsPropertyElementName())
						{
							// collect a value as a new attribute setter
							if (setters.Keys().Contains(GlobalStringKey::Get(name->name)))
							{
								errors.Add(GuiResourceError(resource, element->name.codeRange.start, L"Duplicated property \"" + name->name + L"\"."));
							}
							else
							{
								Ptr<GuiAttSetterRepr::SetterValue> sv = new GuiAttSetterRepr::SetterValue;
								sv->binding = GlobalStringKey::Get(name->binding);
								sv->attPosition = element->codeRange.start;

								if (name->binding == L"set")
								{
									// if the binding is "set", it means that this element is a complete setter element
									Ptr<GuiAttSetterRepr> setter = new GuiAttSetterRepr;
									FillAttSetter(resource, setter, element, errors);
									sv->values.Add(setter);
								}
								else
								{
									// if the binding is not "set", then this is a single-value attribute or a colection attribute
									// fill all data into this attribute
									CollectDefaultAttributes(resource, sv->values, element, errors);
								}

								if (sv->values.Count() > 0)
								{
									setters.Add(GlobalStringKey::Get(name->name), sv);
								}
							}
						}
					}
				}
			}
		}

		void GuiInstanceContext::CollectEvents(Ptr<GuiResourceItem> resource, GuiAttSetterRepr::EventHandlerMap& eventHandlers, Ptr<parsing::xml::XmlElement> xml, GuiResourceError::List& errors)
		{
			if (auto parser = GetParserManager()->GetParser<ElementName>(L"INSTANCE-ELEMENT-NAME"))
			{
				// collect values
				FOREACH(Ptr<XmlElement>, element, XmlGetElements(xml))
				{
					List<Ptr<ParsingError>> parsingErrors;
					auto name = parser->TypedParse(element->name.value, parsingErrors);
					GuiResourceError::Transform(resource, errors, parsingErrors, element->name.codeRange.start);
					if (name)
					{
						if (name->IsEventElementName())
						{
							// collect a value as an event setter
							if (eventHandlers.Keys().Contains(GlobalStringKey::Get(name->name)))
							{
								errors.Add(GuiResourceError(resource, element->name.codeRange.start, L"Duplicated event \"" + name->name + L"\"."));
							}
							else
							{
								// test if there is only one text value in the xml
								if (element->subNodes.Count() == 1)
								{
									if (Ptr<XmlText> text = element->subNodes[0].Cast<XmlText>())
									{
										auto value = MakePtr<GuiAttSetterRepr::EventValue>();
										value->binding = GlobalStringKey::Get(name->binding);
										value->value = text->content.value;
										value->attPosition = element->codeRange.start;
										value->valuePosition = text->content.codeRange.start;
										eventHandlers.Add(GlobalStringKey::Get(name->name), value);
										if (text->content.codeRange.start.row != text->content.codeRange.end.row)
										{
											errors.Add(GuiResourceError(resource, element->codeRange.start, L"Multiple lines script should be contained in a CDATA section."));
										}
									}
									else if (Ptr<XmlCData> text = element->subNodes[0].Cast<XmlCData>())
									{
										auto value = MakePtr<GuiAttSetterRepr::EventValue>();
										value->binding = GlobalStringKey::Get(name->binding);
										value->value = text->content.value;
										value->attPosition = element->codeRange.start;
										value->valuePosition = text->content.codeRange.start;
										value->valuePosition.column += 9; // <![CDATA[
										eventHandlers.Add(GlobalStringKey::Get(name->name), value);
									}
								}
								else
								{
									errors.Add(GuiResourceError(resource, element->codeRange.start, L"Event script should be contained in a text or CDATA section."));
								}
							}
						}
					}
				}
			}
		}

		void GuiInstanceContext::FillAttSetter(Ptr<GuiResourceItem> resource, Ptr<GuiAttSetterRepr> setter, Ptr<parsing::xml::XmlElement> xml, GuiResourceError::List& errors)
		{
			if (auto parser = GetParserManager()->GetParser<ElementName>(L"INSTANCE-ELEMENT-NAME"))
			{
				setter->tagPosition = xml->codeRange.start;

				// collect attributes as setters
				FOREACH(Ptr<XmlAttribute>, att, xml->attributes)
				{
					List<Ptr<ParsingError>> parsingErrors;
					auto name = parser->TypedParse(att->name.value, parsingErrors);
					GuiResourceError::Transform(resource, errors, parsingErrors, att->name.codeRange.start);
					if (name)
					{
						if (name->IsReferenceAttributeName())
						{
							// collect reference attributes
							if (name->name == L"Name")
							{
								setter->instanceName = GlobalStringKey::Get(att->value.value);
							}
						}
						else if (name->IsEnvironmentAttributeName())
						{
							// collect environment variables
							if (setter->environmentVariables.Keys().Contains(GlobalStringKey::Get(name->name)))
							{
								errors.Add(GuiResourceError(resource, att->name.codeRange.start, L"Duplicated environment variable \"" + name->name + L"\"."));
							}
							else
							{
								auto value = MakePtr<GuiAttSetterRepr::EnvVarValue>();
								value->value = att->value.value;
								value->attPosition = att->codeRange.start;
								value->valuePosition = att->value.codeRange.start;
								setter->environmentVariables.Add(GlobalStringKey::Get(name->name), value);
							}
						}
						else if (name->IsPropertyAttributeName())
						{
							// collect attributes setters
							if (setter->setters.Keys().Contains(GlobalStringKey::Get(name->name)))
							{
								errors.Add(GuiResourceError(resource, att->name.codeRange.start, L"Duplicated property \"" + name->name + L"\"."));
							}
							else
							{
								auto sv = MakePtr<GuiAttSetterRepr::SetterValue>();
								sv->binding = GlobalStringKey::Get(name->binding);
								sv->attPosition = att->codeRange.start;
								setter->setters.Add(GlobalStringKey::Get(name->name), sv);

								Ptr<GuiTextRepr> value = new GuiTextRepr;
								value->text = att->value.value;
								sv->values.Add(value);
							}
						}
						else if (name->IsEventAttributeName())
						{
							// collect event setters
							if (setter->eventHandlers.Keys().Contains(GlobalStringKey::Get(name->name)))
							{
								errors.Add(GuiResourceError(resource, att->name.codeRange.start, L"Duplicated event \"" + name->name + L"\"."));
							}
							else
							{
								auto value = MakePtr<GuiAttSetterRepr::EventValue>();
								value->binding = GlobalStringKey::Get(name->binding);
								value->value = att->value.value;
								value->attPosition = att->codeRange.start;
								value->valuePosition = att->value.codeRange.start;
								setter->eventHandlers.Add(GlobalStringKey::Get(name->name), value);
							}
						}
						else
						{
							errors.Add(GuiResourceError(resource, att->name.codeRange.start, L"Unknown attribute name: \"" + att->name.value + L"\"."));
						}
					}
				}

				// collect attributes and events
				CollectAttributes(resource, setter->setters, xml, errors);
				CollectEvents(resource, setter->eventHandlers, xml, errors);
			}
		}

		Ptr<GuiConstructorRepr> GuiInstanceContext::LoadCtor(Ptr<GuiResourceItem> resource, Ptr<parsing::xml::XmlElement> xml, GuiResourceError::List& errors)
		{
			if (auto parser = GetParserManager()->GetParser<ElementName>(L"INSTANCE-ELEMENT-NAME"))
			{
				List<Ptr<ParsingError>> parsingErrors;
				auto ctorName = parser->TypedParse(xml->name.value, parsingErrors);
				GuiResourceError::Transform(resource, errors, parsingErrors, xml->name.codeRange.start);
				if (ctorName)
				{
					if (ctorName->IsCtorName())
					{
						Ptr<GuiConstructorRepr> ctor = new GuiConstructorRepr;
						ctor->typeNamespace = GlobalStringKey::Get(ctorName->namespaceName);
						ctor->typeName = GlobalStringKey::Get(ctorName->name);
						// collect attributes as setters
						FOREACH(Ptr<XmlAttribute>, att, xml->attributes)
						{
							parsingErrors.Clear();
							auto attName = parser->TypedParse(att->name.value, parsingErrors);
							GuiResourceError::Transform(resource, errors, parsingErrors, att->name.codeRange.start);
							if (attName)
							{
								if (attName->IsReferenceAttributeName())
								{
									if (attName->name == L"Style")
									{
										ctor->styleName = att->value.value;
									}
								}
							}
						}
						FillAttSetter(resource, ctor, xml, errors);
						return ctor;
					}
					else
					{
						errors.Add(GuiResourceError(resource, xml->name.codeRange.start, L"Wrong constructor name \"" + xml->name.value + L"\"."));
					}
				}
			}
			return 0;
		}

		Ptr<GuiInstanceContext> GuiInstanceContext::LoadFromXml(Ptr<GuiResourceItem> resource, Ptr<parsing::xml::XmlDocument> xml, GuiResourceError::List& errors)
		{
			Ptr<GuiInstanceContext> context = new GuiInstanceContext;
			context->tagPosition = xml->rootElement->codeRange.start;

			if (xml->rootElement->name.value == L"Instance")
			{
				if (auto codeBehindAttr = XmlGetAttribute(xml->rootElement, L"ref.CodeBehind"))
				{
					context->codeBehind = codeBehindAttr->value.value == L"true";
				}

				// load type name
				if (auto classAttr = XmlGetAttribute(xml->rootElement, L"ref.Class"))
				{
					context->className = classAttr->value.value;
					context->classPosition = classAttr->codeRange.start;
				}

				// load style names
				if (auto styleAttr = XmlGetAttribute(xml->rootElement, L"ref.Styles"))
				{
					SplitBySemicolon(styleAttr->value.value, context->stylePaths);
					context->stylePosition = styleAttr->codeRange.start;
				}

				// load namespaces
				List<Ptr<XmlAttribute>> namespaceAttributes;
				CopyFrom(namespaceAttributes, xml->rootElement->attributes);
				if (!XmlGetAttribute(xml->rootElement, L"xmlns"))
				{
					Ptr<XmlAttribute> att = new XmlAttribute;
					att->name.value = L"xmlns";
					att->value.value =
						L"presentation::controls::Gui*;"
						L"presentation::elements::Gui*Element;"
						L"presentation::compositions::Gui*Composition;"
						L"presentation::compositions::Gui*;"
						L"presentation::templates::Gui*;"
						L"system::*;"
						L"system::reflection::*;"
						L"presentation::*;"
						L"presentation::Gui*;"
						L"presentation::controls::*;"
						L"presentation::controls::list::*;"
						L"presentation::controls::tree::*;"
						L"presentation::elements::*;"
						L"presentation::elements::Gui*;"
						L"presentation::elements::text::*;"
						L"presentation::compositions::*;"
						L"presentation::templates::*";
					namespaceAttributes.Add(att);
				}
				FOREACH(Ptr<XmlAttribute>, att, namespaceAttributes)
				{
					// check if the attribute defines a namespace
					WString attName = att->name.value;
					if (attName.Length() >= 5 && attName.Left(5) == L"xmlns")
					{
						GlobalStringKey ns;
						if (attName.Length() > 6)
						{
							if (attName.Left(6) == L"xmlns:")
							{
								ns = GlobalStringKey::Get(attName.Sub(6, attName.Length() - 6));
							}
							else
							{
								continue;
							}
						}

						// create a data structure for the namespace
						Ptr<NamespaceInfo> info;
						vint index = context->namespaces.Keys().IndexOf(ns);
						if (index == -1)
						{
							info = new NamespaceInfo;
							info->name = ns;
							info->attPosition = att->codeRange.start;
							context->namespaces.Add(ns, info);
						}
						else
						{
							info = context->namespaces.Values()[index];
						}

						// extract all patterns in the namespace, split the value by ';'
						List<WString> patterns;
						SplitBySemicolon(att->value.value, patterns);
						FOREACH(WString, pattern, patterns)
						{
							// add the pattern to the namespace
							Ptr<GuiInstanceNamespace> ns = new GuiInstanceNamespace;
							Pair<vint, vint> star = INVLOC.FindFirst(pattern, L"*", Locale::None);
							if (star.key == -1)
							{
								ns->prefix = pattern;
							}
							else
							{
								ns->prefix = pattern.Sub(0, star.key);
								ns->postfix = pattern.Sub(star.key + star.value, pattern.Length() - star.key - star.value);
							}
							info->namespaces.Add(ns);
						}
					}
				}

				// load instance
				FOREACH(Ptr<XmlElement>, element, XmlGetElements(xml->rootElement))
				{
					if (element->name.value == L"ref.Parameter")
					{
						auto attName = XmlGetAttribute(element, L"Name");
						auto attClass = XmlGetAttribute(element, L"Class");
						if (attName && attClass)
						{
							auto parameter = MakePtr<GuiInstanceParameter>();
							parameter->name = GlobalStringKey::Get(attName->value.value);
							parameter->className = GlobalStringKey::Get(attClass->value.value);
							parameter->tagPosition = element->codeRange.start;
							context->parameters.Add(parameter);
						}
						else
						{
							errors.Add(GuiResourceError(resource, element->name.codeRange.start, L"ref.Parameter requires the following attributes existing at the same time: Name, Class."));
						}
					}
					else if (element->name.value == L"ref.Members")
					{
						if (element->subNodes.Count() == 1)
						{
							if (auto cdata = element->subNodes[0].Cast<XmlCData>())
							{
								context->memberScript = cdata->content.value;
								context->memberPosition = cdata->codeRange.start;
								context->memberPosition.column += 9; // <![CDATA[
								goto MEMBERSCRIPT_SUCCESS;
							}
						}
						errors.Add(GuiResourceError(resource, element->codeRange.start, L"Script should be contained in a CDATA section."));
					MEMBERSCRIPT_SUCCESS:;
					}
					else if (!context->instance)
					{
						context->instance = LoadCtor(resource, element, errors);
					}
				}
			}

			return context->instance ? context : nullptr;
		}

		Ptr<parsing::xml::XmlDocument> GuiInstanceContext::SaveToXml()
		{
			auto xmlInstance = MakePtr<XmlElement>();
			xmlInstance->name.value = L"Instance";

			{
				auto attCodeBehind = MakePtr<XmlAttribute>();
				attCodeBehind->name.value = L"ref.CodeBehind";
				attCodeBehind->value.value = codeBehind ? L"true" : L"false";
				xmlInstance->attributes.Add(attCodeBehind);
			}

			auto attClass = MakePtr<XmlAttribute>();
			attClass->name.value = L"ref.Class";
			attClass->value.value = className;
			xmlInstance->attributes.Add(attClass);

			for (vint i = 0; i < namespaces.Count(); i++)
			{
				auto key = namespaces.Keys()[i];
				auto value = namespaces.Values()[i];

				auto xmlns = MakePtr<XmlAttribute>();
				xmlns->name.value = L"xmlns";
				if (key != GlobalStringKey::Empty)
				{
					xmlns->name.value += L":" + key.ToString();
				}
				xmlInstance->attributes.Add(xmlns);

				for (vint j = 0; j < value->namespaces.Count(); j++)
				{
					auto ns = value->namespaces[j];
					if (j != 0)
					{
						xmlns->value.value += L";";
					}
					xmlns->value.value += ns->prefix + L"*" + ns->postfix;
				}
			}

			FOREACH(Ptr<GuiInstanceParameter>, parameter, parameters)
			{
				auto xmlParameter = MakePtr<XmlElement>();
				xmlParameter->name.value = L"ref.Parameter";
				xmlInstance->subNodes.Add(xmlParameter);

				auto attName = MakePtr<XmlAttribute>();
				attName->name.value = L"Name";
				attName->value.value = parameter->name.ToString();
				xmlParameter->attributes.Add(attName);

				auto attClass = MakePtr<XmlAttribute>();
				attClass->name.value = L"Class";
				attClass->value.value = parameter->className.ToString();
				xmlParameter->attributes.Add(attClass);
			}

			if (memberScript != L"")
			{
				auto xmlMembers = MakePtr<XmlElement>();
				xmlMembers->name.value = L"ref.Members";
				xmlInstance->subNodes.Add(xmlMembers);

				auto text = MakePtr<XmlCData>();
				text->content.value = memberScript;
				xmlMembers->subNodes.Add(text);
			}

			if (stylePaths.Count() > 0)
			{
				auto attStyles = MakePtr<XmlAttribute>();
				attStyles->name.value = L"ref.Styles";
				xmlInstance->attributes.Add(attStyles);

				for (vint j = 0; j < stylePaths.Count(); j++)
				{
					if (j != 0)
					{
						attStyles->value.value += L";";
					}
					attStyles->value.value += stylePaths[j];
				}
			}

			instance->FillXml(xmlInstance);

			auto doc = MakePtr<XmlDocument>();
			doc->rootElement = xmlInstance;
			return doc;
		}

		bool GuiInstanceContext::ApplyStyles(Ptr<GuiResourceItem> resource, Ptr<GuiResourcePathResolver> resolver, GuiResourceError::List& errors)
		{
			if (!appliedStyles)
			{
				appliedStyles = true;

				List<Ptr<GuiInstanceStyle>> styles;
				FOREACH(WString, uri, stylePaths)
				{
					WString protocol, path;
					if (IsResourceUrl(uri, protocol, path))
					{
						if (auto styleContext = resolver->ResolveResource(protocol, path).Cast<GuiInstanceStyleContext>())
						{
							CopyFrom(styles, styleContext->styles, true);
						}
						else
						{
							errors.Add(GuiResourceError(resource, stylePosition, L"Failed to find the style referred in attribute \"ref.Styles\": \"" + uri + L"\"."));
						}
					}
					else
					{
						errors.Add(GuiResourceError(resource, stylePosition, L"Invalid path in attribute \"ref.Styles\": \"" + uri + L"\"."));
					}
				}

				FOREACH(Ptr<GuiInstanceStyle>, style, styles)
				{
					List<Ptr<GuiConstructorRepr>> output;
					ExecuteQuery(style->query, this, output);
					FOREACH(Ptr<GuiConstructorRepr>, ctor, output)
					{
						ApplyStyle(style, ctor);
					}
				}

				return true;
			}
			else
			{
				return false;
			}
		}

/***********************************************************************
GuiInstanceStyle
***********************************************************************/

		namespace visitors
		{
			class SetStyleMarkVisitor : public Object, public GuiValueRepr::IVisitor
			{
			public:
				void Visit(GuiTextRepr* repr)override
				{
					repr->fromStyle = true;
				}

				void Visit(GuiAttSetterRepr* repr)override
				{
					repr->fromStyle = true;
					FOREACH(Ptr<GuiAttSetterRepr::SetterValue>, value, repr->setters.Values())
					{
						FOREACH(Ptr<GuiValueRepr>, subValue, value->values)
						{
							subValue->Accept(this);
						}
					}
					FOREACH(Ptr<GuiAttSetterRepr::EventValue>, value, repr->eventHandlers.Values())
					{
						value->fromStyle = true;
					}
				}

				void Visit(GuiConstructorRepr* repr)override
				{
					Visit((GuiAttSetterRepr*)repr);
				}
			};
		}
		using namespace visitors;

		Ptr<GuiInstanceStyle> GuiInstanceStyle::LoadFromXml(Ptr<GuiResourceItem> resource, Ptr<parsing::xml::XmlElement> xml, GuiResourceError::List& errors)
		{
			auto style = MakePtr<GuiInstanceStyle>();
			if (auto pathAttr = XmlGetAttribute(xml, L"ref.Path"))
			{
				List<Ptr<ParsingError>> parsingErrors;
				auto parser = GetParserManager()->GetParser<GuiIqQuery>(L"INSTANCE-QUERY");
				auto query = parser->TypedParse(pathAttr->value.value, parsingErrors);
				GuiResourceError::Transform(resource, errors, parsingErrors, pathAttr->value.codeRange.start, { 0,1 });
				if (!query) return nullptr;
				style->query = query;
			}
			else
			{
				errors.Add(GuiResourceError(resource, xml->codeRange.start, L"Missing attribute \"ref.Path\" in <Style>."));
			}
			style->setter = MakePtr<GuiAttSetterRepr>();
			GuiInstanceContext::FillAttSetter(resource, style->setter, xml, errors);

			SetStyleMarkVisitor visitor;
			style->setter->Accept(&visitor);
			return style;
		}

		Ptr<parsing::xml::XmlElement> GuiInstanceStyle::SaveToXml()
		{
			auto xmlStyle = MakePtr<XmlElement>();
			xmlStyle->name.value = L"Style";

			auto attPath = MakePtr<XmlAttribute>();
			attPath->name.value = L"ref.Path";
			{
				MemoryStream stream;
				{
					StreamWriter writer(stream);
					GuiIqPrint(query, writer);
				}
				stream.SeekFromBegin(0);
				{
					StreamReader reader(stream);
					attPath->value.value = reader.ReadToEnd();
				}
			}
			xmlStyle->attributes.Add(attPath);

			setter->FillXml(xmlStyle);
			return xmlStyle;
		}

/***********************************************************************
GuiInstanceStyleContext
***********************************************************************/

		Ptr<GuiInstanceStyleContext> GuiInstanceStyleContext::LoadFromXml(Ptr<GuiResourceItem> resource, Ptr<parsing::xml::XmlDocument> xml, GuiResourceError::List& errors)
		{
			auto context = MakePtr<GuiInstanceStyleContext>();
			if (xml->rootElement->name.value == L"Styles")
			{
				FOREACH(Ptr<XmlElement>, styleElement, XmlGetElements(xml->rootElement))
				{
					if (styleElement->name.value == L"Style")
					{
						if (auto style = GuiInstanceStyle::LoadFromXml(resource, styleElement, errors))
						{
							context->styles.Add(style);
						}
					}
					else
					{
						errors.Add(GuiResourceError(styleElement->codeRange.start, L"Unknown element in <Styles>: \"" + styleElement->name.value + L"\"."));
					}
				}
			}
			else
			{
				errors.Add(GuiResourceError(resource, xml->rootElement->codeRange.start, L"The root element of instance styles should be \"Styles\"."));
			}
			return context;
		}

		Ptr<parsing::xml::XmlDocument> GuiInstanceStyleContext::SaveToXml()
		{
			auto xmlStyles = MakePtr<XmlElement>();
			xmlStyles->name.value = L"Styles";

			FOREACH(Ptr<GuiInstanceStyle>, style, styles)
			{
				xmlStyles->subNodes.Add(style->SaveToXml());
			}

			auto doc = MakePtr<XmlDocument>();
			doc->rootElement = xmlStyles;
			return doc;
		}
	}
}