/**************************************************************************/
/*! 
    @file     PackageDescriptor.h
    @author   Sebastien Warin (http://sebastien.warin.fr)
    @version  2.4.18186

    @section LICENSE

    Constellation License Agreement

    Copyright (c) 2015-2018, Sebastien Warin
    All rights reserved.

    By receiving, opening the file package, and/or using Constellation 1.8("Software")
    containing this software, you agree that this End User User License Agreement(EULA)
    is a legally binding and valid contract and agree to be bound by it.
    You agree to abide by the intellectual property laws and all of the terms and
    conditions of this Agreement.
    http://www.myconstellation.io/license.txt

*/
/**************************************************************************/

#ifndef _CONSTELLATION_PACKAGE_DESCRIPTOR_
#define _CONSTELLATION_PACKAGE_DESCRIPTOR_

#include <type_traits>
#include <ArduinoJson.h>

#define ENUM_TYPE       0
#define PROPERTY_TYPE   1
#define PARAMETER_TYPE  2

typedef struct {
    const char* name;
    const char* type;
    const char* description;
    bool isOptional;
    JsonVariant defaultValue;
} MemberInfo;

template<typename T>
class TypeDescriptorBase
{
  private:
    const char* _description;
    bool _isHidden;
    LinkedList<MemberInfo> *_members = new LinkedList<MemberInfo>();
    
  protected:
    template<typename TParam>
    const char* getTypename() {
        if (std::is_same<TParam, bool>::value) {
            return "System.Boolean";
        }
        else if (std::is_same<TParam, float>::value) {
            return "System.Float";
        }
        else if (std::is_same<TParam, double>::value) {
            return "System.Double";
        }        
        else if (std::is_same<TParam, byte>::value) {
            return "System.Byte";
        }
        else if (std::is_same<TParam, signed short>::value || std::is_same<TParam, unsigned short>::value) {
            return "System.Int16";
        }
        else if (std::is_same<TParam, signed long>::value || std::is_same<TParam, unsigned long>::value) {
            return "System.Int64";
        }
        else if (std::is_same<TParam, signed int>::value || std::is_same<TParam, unsigned int>::value) {
            return "System.Int32";
        }
        else if (std::is_same<TParam, String>::value || std::is_same<TParam, const char*>::value || std::is_same<TParam, char*>::value) {
            return "System.String";
        }
        else if (std::is_same<TParam, signed char>::value || std::is_same<TParam, unsigned char>::value) {
            return "System.Char";
        }
        else {
            return "System.Object";
        }
    };

  public:
    virtual void fillJsonObject(JsonObject& mcObject) = 0;
    void fillJsonObject(JsonObject& mcObject, uint8_t memberType) {
        if(this->_description != NULL) {
            mcObject["Description"] = this->_description;
        }
        JsonArray& parameters = mcObject.createNestedArray(memberType == PARAMETER_TYPE ? "Parameters" : "Properties");
        for(int i = 0; i < this->_members->size(); i++) {
            MemberInfo member = this->_members->get(i);
            JsonObject& parameterObject = parameters.createNestedObject();
            parameterObject["Name"] = member.name;
            parameterObject["TypeName"] = member.type;
            parameterObject["Type"] = memberType;
            if(member.isOptional) {
                parameterObject["IsOptional"] = true;
                parameterObject["DefaultValue"] = member.defaultValue;
            }
            if(member.description != NULL) {
                parameterObject["Description"] = member.description;
            }
        }
    };

    virtual T& empty() {
        this->_description = NULL;
        this->_isHidden = false;
        return (T&)*this;
    };

    T& setDescription(const char * description){
        this->_description = description;
        return (T&)*this;
    };
    
    T& setHidden(bool hidden = true) {
        this->_isHidden = hidden;
        return (T&)*this;
    };

    bool isHidden() {
        return this->_isHidden;
    }

    T& addOptionalMember(const char * name, JsonVariant defaultValue, const char * description) {
        if(defaultValue.is<bool>()) {
            return addMember(name, "System.Boolean", defaultValue, description);
        }
        else if(defaultValue.is<float>()) {
            return addMember(name, "System.Float", defaultValue, description);
        }
        else if(defaultValue.is<double>()) {
            return addMember(name, "System.Double", defaultValue, description);
        }
        else if(defaultValue.is<byte>()) {
            return addMember(name, "System.Byte", defaultValue, description);
        }
        else if(defaultValue.is<signed short>() || defaultValue.is<unsigned short>()) {
            return addMember(name, "System.Int16",  defaultValue, description);
        }
        else if(defaultValue.is<signed long>() || defaultValue.is<unsigned long>()) {
            return addMember(name, "System.Int64", defaultValue, description);
        }
        else if(defaultValue.is<signed int>() || defaultValue.is<unsigned int>()) {
            return addMember(name, "System.Int32", defaultValue, description);
        }
        else if(defaultValue.is<const char*>() || defaultValue.is<char*>()) {
            return addMember(name, "System.String", defaultValue, description);
        }
        else if(defaultValue.is<signed char>() || defaultValue.is<unsigned char>()) {
            return addMember(name, "System.Char", defaultValue, description);
        }
        else {
            return addMember(name, "System.Object", defaultValue, description);
        }
    };
    template<typename TParam>
    T& addMember(const char * name, const char * description) {
        return addMember(name, getTypename<TParam>(), description);
    };
    T& addMember(const char * name, const char * type, const char * description) {
        return addMember(name, type, false, NULL, description);
    };
    T& addMember(const char * name, const char * type, JsonVariant defaultValue, const char * description) {
        return addMember(name, type, true, defaultValue, description);
    };
    T& addMember(const char * name, const char * type, bool isOptional, JsonVariant defaultValue, const char * description) {
        MemberInfo member;
        member.name = name;
        member.type = type;
        member.description = description;
        member.isOptional = isOptional;
        member.defaultValue = defaultValue;
        this->_members->add(member);
        return (T&)*this;
    };
};

class MessageCallbackDescriptor : public TypeDescriptorBase<MessageCallbackDescriptor>
{
  private:
    const char* _returnType;
    
  public:
    MessageCallbackDescriptor() {
        empty();
    };

    void fillJsonObject(JsonObject& mcObject) {
        TypeDescriptorBase<MessageCallbackDescriptor>::fillJsonObject(mcObject, PARAMETER_TYPE);
        if(this->_returnType != NULL) {
            mcObject["ResponseType"] = this->_returnType;
        }
    }    
    
    virtual MessageCallbackDescriptor& empty() {
        TypeDescriptorBase<MessageCallbackDescriptor>::empty();
        this->_returnType = NULL;
        return *this;
    };
    
    template<typename TParam>
    MessageCallbackDescriptor& setReturnType() {
        return setReturnType(getTypename<TParam>());
    };
    MessageCallbackDescriptor& setReturnType(const char * returnType){
        this->_returnType = returnType;
        return *this;
    };

    template<typename TParam>
    MessageCallbackDescriptor& addParameter(const char * name, const char * description = NULL) {
        return TypeDescriptorBase<MessageCallbackDescriptor>::addMember<TParam>(name, description);
    };
    MessageCallbackDescriptor& addParameter(const char * name, const char * type, const char * description = NULL) {
        return TypeDescriptorBase<MessageCallbackDescriptor>::addMember(name, type, description);
    };
    template<typename TParam> 
    MessageCallbackDescriptor& addOptionalParameter(const char * name, TParam defaultValue, const char * description = NULL) {
        return TypeDescriptorBase<MessageCallbackDescriptor>::addOptionalMember(name, defaultValue, description);
    };
};

class TypeDescriptor : public TypeDescriptorBase<TypeDescriptor>
{
  public:
    TypeDescriptor() {
        empty();
    };

    void fillJsonObject(JsonObject& mcObject) {
        TypeDescriptorBase<TypeDescriptor>::fillJsonObject(mcObject, PROPERTY_TYPE);
    };

    template<typename TParam>
    TypeDescriptor& addProperty(const char * name, const char * description = NULL) {
        return TypeDescriptorBase<TypeDescriptor>::addMember<TParam>(name, description);
    };
    TypeDescriptor& addProperty(const char * name, const char * type, const char * description = NULL) {
        return TypeDescriptorBase<TypeDescriptor>::addMember(name, type, description);
    };
};

#endif
