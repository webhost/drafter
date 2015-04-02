//
//  SerializeAST.cc
//  drafter
//
//  Created by Pavan Kumar Sunkara on 18/01/15.
//  Copyright (c) 2015 Apiary Inc. All rights reserved.
//

#include "StringUtility.h"
#include "SerializeAST.h"

using namespace drafter;

using snowcrash::AssetRole;
using snowcrash::BodyExampleAssetRole;
using snowcrash::BodySchemaAssetRole;

using snowcrash::Element;
using snowcrash::Elements;
using snowcrash::KeyValuePair;
using snowcrash::Metadata;
using snowcrash::Header;
using snowcrash::Reference;
using snowcrash::DataStructure;
using snowcrash::Asset;
using snowcrash::Payload;
using snowcrash::Value;
using snowcrash::Values;
using snowcrash::Parameter;
using snowcrash::TransactionExample;
using snowcrash::Request;
using snowcrash::Response;
using snowcrash::Action;
using snowcrash::Resource;
using snowcrash::Blueprint;

template<typename T> struct Wrap;

struct Proxy {
    sos::Object object;

    Proxy(){}

    template<typename T> Proxy(const std::string& key, const T& value) {
        set(key, value);
    }

    template<typename T> Proxy(const T& value) : object(Wrap<T>()(value)) {
    }

    template<typename T>
    Proxy& operator()(const std::string& key, const T& value) {
        return set(key, value);
    }

    template<typename T>
    Proxy& set(const std::string& key, const T& value) {
        Wrap<T> Wrapper;
        object.set(key, Wrapper(value));
        return *this;
    }

    operator sos::Object() const { return object; }
};

struct true_type {};
struct false_type {};

template<typename T> struct is_vector : public false_type {};
template<typename T, typename A> struct is_vector<std::vector<T, A> > : public true_type {};

template<typename T>
struct Wrap {
    sos::Array operator()(const T& value, true_type) const {
        typedef typename T::value_type value_type;
        typedef Wrap<value_type> wrapper_type;
        wrapper_type wrap;

        return WrapCollection<value_type>()(value, wrap);
    }

    sos::Array operator()(const T& value) const {
        return (*this)(value, is_vector<T>());
    }
};

template<> struct Wrap<std::string>{
    sos::String operator()(const std::string& val) const {
        return sos::String(val);
    }
};

template<> struct Wrap<bool>{
    sos::Boolean operator()(const bool& val) const {
        return sos::Boolean(val);
    }
};

template<> struct Wrap<Proxy>{
    sos::Object operator()(const Proxy& val) const {
        return val.object;
    }
};

template<> struct Wrap<sos::Array>{
    sos::Array operator()(const sos::Array& val) const {
        return val;
    }
};

template<> struct Wrap<mson::Value>{
    sos::Object operator()(const mson::Value& value) const {
        return Proxy(SerializeKey::Literal, value.literal)
                    (SerializeKey::Variable, value.variable);
    }
};

template<> struct Wrap<mson::Symbol>{
    sos::Object operator()(const mson::Symbol& symbol) const {
        return Proxy(SerializeKey::Literal, symbol.literal)
                    (SerializeKey::Variable, symbol.variable);
    }
};

static const std::string TypeNameToString(const mson::TypeName& typeName)
{
    switch (typeName.base) {

        case mson::BooleanTypeName:
            return "boolean";

        case mson::StringTypeName:
            return "string";

        case mson::NumberTypeName:
            return "number";

        case mson::ArrayTypeName:
            return "array";

        case mson::EnumTypeName:
            return "enum";

        case mson::ObjectTypeName:
            return "object";

        default:
            break;
    }

    return std::string();
}

template<> struct Wrap<mson::TypeName> {
    sos::Base operator()(const mson::TypeName& typeName) const {
        if (typeName.empty()) {
            return sos::Null();
        }

        if (typeName.base != mson::UndefinedTypeName) {
            return sos::String(TypeNameToString(typeName));
        }

        return Wrap<mson::Symbol>()(typeName.symbol);
    }
};

template<> struct Wrap<mson::TypeAttributes> {
    sos::Array operator()(const mson::TypeAttributes& typeAttributes)
    {
        sos::Array typeAttributesArray;

        if (typeAttributes & mson::RequiredTypeAttribute) {
            typeAttributesArray.push(sos::String("required"));
        }
        else if (typeAttributes & mson::OptionalTypeAttribute) {
            typeAttributesArray.push(sos::String("optional"));
        }
        else if (typeAttributes & mson::DefaultTypeAttribute) {
            typeAttributesArray.push(sos::String("default"));
        }
        else if (typeAttributes & mson::SampleTypeAttribute) {
            typeAttributesArray.push(sos::String("sample"));
        }
        else if (typeAttributes & mson::FixedTypeAttribute) {
            typeAttributesArray.push(sos::String("fixed"));
        }

        return typeAttributesArray;
    }
};

template<> struct Wrap<mson::TypeSpecification> {
    sos::Object operator()(const mson::TypeSpecification& value) const {
        return Proxy(SerializeKey::Name, value.name)
                    (SerializeKey::NestedTypes, value.nestedTypes);
    }
};

template<> struct Wrap<mson::TypeDefinition> {
    sos::Object operator()(const mson::TypeDefinition& typeDefinition) const {
        return Proxy(SerializeKey::TypeSpecification, typeDefinition.typeSpecification)
                    (SerializeKey::Attributes, typeDefinition.attributes);
    }
};

template<> struct Wrap<mson::ValueDefinition> {
    sos::Object operator()(const mson::ValueDefinition& value) const {
        return Proxy(SerializeKey::Values, value.values)
                    (SerializeKey::TypeDefinition, value.typeDefinition);
    }
};

template<> struct Wrap<mson::PropertyName> {
    sos::Object operator()(const mson::PropertyName& value) const {
        Proxy object;
        if (!value.literal.empty()) {
            object.set(SerializeKey::Literal, value.literal);
        }
        else if (!value.variable.empty()) {
            object.set(SerializeKey::Variable, value.variable);
        }
        return object;
    }
};

static const std::string TypeSectionClassToString(const mson::TypeSection::Class& klass)
{
    switch (klass) {
        case mson::TypeSection::BlockDescriptionClass:
            return "blockDescription";

        case mson::TypeSection::MemberTypeClass:
            return "memberType";

        case mson::TypeSection::SampleClass:
            return "sample";

        case mson::TypeSection::DefaultClass:
            return "default";

        default:
            break;
    }

    return std::string();
}

static const std::string AssetRoleToString(const AssetRole& role)
{
    std::string str;

    switch (role) {
        case BodyExampleAssetRole:
            str = "bodyExample";
            break;

        case BodySchemaAssetRole:
            str = "bodySchema";
            break;

        default:
            break;
    }

    return std::string(str);
}

static const std::string ElementClassToString(const Element::Class& element)
{
    switch (element) {
        case Element::CategoryElement:
            return "category";

        case Element::CopyElement:
            return "copy";

        case Element::ResourceElement:
            return "resource";

        case Element::DataStructureElement:
            return "dataStructure";

        case Element::AssetElement:
            return "asset";

        default:
            break;
    }

    return std::string();
}

template<> struct Wrap<KeyValuePair> {
    sos::Object operator()(const KeyValuePair& value) const {
        return Proxy(SerializeKey::Name, value.first)
                    (SerializeKey::Value, value.second);
    }
};

template<> struct Wrap<Reference> {
    sos::Object operator()(const Reference& value) const {
        return Proxy(SerializeKey::Id, value.id);
    }
};

template<> struct Wrap<mson::PropertyMember> {
    sos::Object operator()(const mson::PropertyMember& value) const {
        return Proxy(SerializeKey::Name, value.name)
                    (SerializeKey::Description, value.description)
                    (SerializeKey::ValueDefinition, value.valueDefinition)
                    (SerializeKey::Sections, value.sections);
    }
};

template<> struct Wrap<mson::ValueMember> {
    sos::Object operator()(const mson::ValueMember& value) const {
        return Proxy(SerializeKey::Description, value.description)
                    (SerializeKey::ValueDefinition, value.valueDefinition)
                    (SerializeKey::Sections, value.sections);
    }
};

template<> struct Wrap<mson::Element> {
    sos::Object operator()(const mson::Element& element) const {
        Proxy object;
        std::string klass;

        switch (element.klass) {

            case mson::Element::PropertyClass:
            {
                klass = "property";
                object.set(SerializeKey::Content, element.content.property);
                break;
            }

            case mson::Element::ValueClass:
            {
                klass = "value";
                object.set(SerializeKey::Content, element.content.value);
                break;
            }

            case mson::Element::MixinClass:
            {
                klass = "mixin";
                object.set(SerializeKey::Content, element.content.mixin);
                break;
            }

            case mson::Element::OneOfClass:
            {
                klass = "oneOf";
                object.set(SerializeKey::Content, element.content.oneOf());
                break;
            }

            case mson::Element::GroupClass:
            {
                klass = "group";
                object.set(SerializeKey::Content, element.content.elements());
                break;
            }

            default:
                break;
        }

        object.set(SerializeKey::Class, klass);

        return object;
    }
};

template<> struct Wrap<mson::TypeSection> {
    sos::Object operator()(const mson::TypeSection& section) const {
        Proxy object;

        object.set(SerializeKey::Class, TypeSectionClassToString(section.klass));

        if (!section.content.description.empty()) {
            object.set(SerializeKey::Content, section.content.description);
        }
        else if (!section.content.value.empty()) {
            object.set(SerializeKey::Content, section.content.value);
        }
        else if (!section.content.elements().empty()) {
            object.set(SerializeKey::Content, section.content.elements());
        }

        return object;
        }
};

template<> struct Wrap<DataStructure> {
    sos::Object operator()(const DataStructure& dataStructure) const {
        return Proxy(SerializeKey::Element, ElementClassToString(Element::DataStructureElement))
                    (SerializeKey::Name, dataStructure.name)
                    (SerializeKey::TypeDefinition, dataStructure.typeDefinition)
                    (SerializeKey::Sections, dataStructure.sections);
    }
};

sos::Object WrapAsset(const Asset& asset, const AssetRole& role)
{
    return Proxy(SerializeKey::Element, ElementClassToString(Element::AssetElement))
                (SerializeKey::Attributes, 
                    Proxy(SerializeKey::Role, AssetRoleToString(role))
                )
                (SerializeKey::Content, asset);
}

template<> struct Wrap<Payload> {
    sos::Object operator()(const Payload& payload) const {
        Proxy object;

        // Reference
        if (!payload.reference.id.empty()) {
            object.set(SerializeKey::Reference, payload.reference);
        }

        object
            .set(SerializeKey::Name, payload.name)
            .set(SerializeKey::Description, payload.description)
            .set(SerializeKey::Headers, payload.headers)
            .set(SerializeKey::Body, payload.body)
            .set(SerializeKey::Schema, payload.schema);

        sos::Array content;

        if (!payload.attributes.empty()) {
            content.push(Wrap<DataStructure>()(payload.attributes));
        }

        if (!payload.body.empty()) {
            content.push(WrapAsset(payload.body, BodyExampleAssetRole));
        }

        if (!payload.schema.empty()) {
            content.push(WrapAsset(payload.schema, BodySchemaAssetRole));
        }

        object.set(SerializeKey::Content, content);

        return object;
    }
};

// Value is alias for std::string
sos::Object WrapValue(const Value& value) {
    return Proxy(SerializeKey::Value, value);
}
template<> struct Wrap<Values> {
    sos::Array operator()(const Values& value) const {
        return WrapCollection<Values>()(value, WrapValue);
    }
};

template<> struct Wrap<Parameter> {
    sos::Object operator()(const Parameter& parameter) const {
        return Proxy(SerializeKey::Name, parameter.name)
                    (SerializeKey::Description, parameter.description)
                    (SerializeKey::Type, parameter.type)
                    (SerializeKey::Required, bool(parameter.use != snowcrash::OptionalParameterUse))
                    (SerializeKey::Default, parameter.defaultValue)
                    (SerializeKey::Example, parameter.exampleValue)
                    (SerializeKey::Values, parameter.values);
    }
};

template<> struct Wrap<TransactionExample> {
    sos::Object operator()(const TransactionExample& example) const {
        return Proxy(SerializeKey::Name, example.name)
                    (SerializeKey::Description, example.description)
                    (SerializeKey::Requests, example.requests)
                    (SerializeKey::Responses, example.responses);
    }
};

template<> struct Wrap<Action> {
    sos::Object operator()(const Action& action) const {
        sos::Array content;

        if (!action.attributes.empty()) {
            content.push(Wrap<DataStructure>()(action.attributes));
        }

        return Proxy(SerializeKey::Name, action.name)
                    (SerializeKey::Description, action.description)
                    (SerializeKey::Method, action.method)
                    (SerializeKey::Parameters, action.parameters)
                    (SerializeKey::Attributes, 
                        Proxy(SerializeKey::Relation, action.relation.str)
                             (SerializeKey::URITemplate, action.uriTemplate)
                    )
                    (SerializeKey::Content, content)
                    (SerializeKey::Examples, action.examples);
    }
};

template<> struct Wrap<Resource> {
    sos::Object operator()(const Resource& resource) const {
        sos::Array content;

        if (!resource.attributes.empty()) {
            content.push(Wrap<DataStructure>()(resource.attributes));
        }

        return Proxy(SerializeKey::Element, ElementClassToString(Element::ResourceElement))
                    (SerializeKey::Name, resource.name)
                    (SerializeKey::Description, resource.description)
                    (SerializeKey::URITemplate, resource.uriTemplate)
                    (SerializeKey::Model, resource.model.name.empty() ? Proxy() : Proxy(resource.model))
                    (SerializeKey::Parameters, resource.parameters)
                    (SerializeKey::Actions, resource.actions)
                    (SerializeKey::Content, content);
    }
};

sos::Object WrapResourceGroup(const Element& resourceGroup)
{
    Proxy object;

    // Name
    object.set(SerializeKey::Name, resourceGroup.attributes.name);

    // Description && Resources
    std::string description;
    sos::Array resources;

    for (Elements::const_iterator it = resourceGroup.content.elements().begin();
         it != resourceGroup.content.elements().end();
         ++it) {

        if (it->element == Element::ResourceElement) {
            resources.push(Wrap<Resource>()(it->content.resource));
        }
        else if (it->element == Element::CopyElement) {

            if (!description.empty()) {
                snowcrash::TwoNewLines(description);
            }

            description += it->content.copy;
        }
    }

    object.set(SerializeKey::Description, description);
    object.set(SerializeKey::Resources, resources);

    return object;
}

template<> struct Wrap<Element> {
    sos::Object operator()(const Element& element) const {
        Proxy object;

        object.set(SerializeKey::Element, ElementClassToString(element.element));

        if (!element.attributes.name.empty()) {
            object.set(SerializeKey::Attributes, 
                Proxy(SerializeKey::Name, element.attributes.name)
            );
        }

        switch (element.element) {
            case Element::CopyElement:
            {
                object.set(SerializeKey::Content, element.content.copy);
                break;
            }

            case Element::CategoryElement:
            {
                object.set(SerializeKey::Content, element.content.elements());
                break;
            }

            case Element::DataStructureElement:
            {
                return Wrap<DataStructure>()(element.content.dataStructure);
            }

            case Element::ResourceElement:
            {
                return Wrap<Resource>()(element.content.resource);
            }

            default:
                break;
        }

        return object;
    }
};

bool IsElementResourceGroup(const Element& element)
{
    return element.element == Element::CategoryElement && element.category == Element::ResourceGroupCategory;
}

sos::Object drafter::WrapBlueprint(const Blueprint& blueprint)
{
    Proxy object;

    object
        .set(SerializeKey::Version, std::string(AST_SERIALIZATION_VERSION))
        .set(SerializeKey::Metadata, blueprint.metadata)
        .set(SerializeKey::Name, blueprint.name)
        .set(SerializeKey::Description, blueprint.description)
        .set(SerializeKey::Element, ElementClassToString(blueprint.element));

    object.object.set(SerializeKey::ResourceGroups, 
                        WrapCollection<Element>()(blueprint.content.elements(), WrapResourceGroup, IsElementResourceGroup));

    object.set(SerializeKey::Content, blueprint.content.elements());

    return object;
}
