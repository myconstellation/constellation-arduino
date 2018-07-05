/**************************************************************************/
/*! 
    @file     BaseDefinitions.h
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

#ifndef _CONSTELLATION_DEFINITIONS_
#define _CONSTELLATION_DEFINITIONS_

#define HTTP_OK 200
#define HTTP_NO_CONTENT 204
#define HTTP_SERVER_ERROR 500

#define DEFAULT_HTTP_USERAGENT "ArduinoLib/2.4"

#define WILDCARD "*"

#define MESSAGE_CALLBACK_SIGNATURE void (*msgCallback)(JsonObject&)
#define MESSAGE_CALLBACK_WCONTEXT_SIGNATURE void (*msgCallbackWithContext)(JsonObject&, MessageContext)
#define STATEOBJECT_CALLBACK_SIGNATURE void (*soCallback)(JsonObject&)

enum ScopeType : uint8_t {
    None = 0,
    Group = 1,
    Package = 2,
    Sentinel = 3,
    Other = 4,
    All = 5
};

enum LogLevel : uint8_t {
    LevelNone = 0,
    LevelInfo = 1,
    LevelWarn = 2,
    LevelError = 3
};

enum SenderType : uint8_t {
    ConsumerHub = 0,
    ConsumerHttp = 1,
    ConstellationHub = 2,
    ConstellationHttp = 3
};

enum DescriptorType : uint8_t {
    MessageCallbackType = 1,
    StateObjectType = 2
};

enum DebugMode : uint8_t {
    Off = 0,
    Error = 1,
    Info = 2,
    Debug = 3,
    Trace = 4
};

typedef struct {
    SenderType  type;
    const char* friendlyName;
    const char* connectionId;
} MessageSender;

typedef struct {
    const char* sagaId;
    bool        isSaga;
    const char* messageKey;
    MessageSender   sender;
    ScopeType   scope;
} MessageContext;

#endif