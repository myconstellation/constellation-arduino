/**************************************************************************/
/*!
    @file     Constellation.h
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

#ifndef _CONSTELLATION_
#define _CONSTELLATION_

#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#if defined(__arm__) 
#include <cstdarg>
#endif

#include <base64.h>
#include <Client.h>
#include <ArduinoJson.h>
#include "BaseDefinitions.h"
#include "BufferedPrint.h"
#include "LinkedList.h"
#include "PackageDescriptor.h"

#define NETCLIENT_BUFFER_SIZE 256
#define STRING_FORMAT_BUFFER 1024
#define JSON_PARSER_BUFFER_SIZE 2560
#define DEFAULT_SUBSCRIPTION_TIMEOUT 60000
#define DEFAULT_SUBSCRIPTION_LIMIT 1
#define SUBSCRIPTIONID_SIZE 36
#define DEFAULT_REQUEST_TIMEOUT 5000

template<typename TNetworkClass>
class Constellation
{
  private:
    TNetworkClass _netClient, _netClientSO, _netClientMsg;
    const char* _constellationHost;
    uint16_t _constellationPort;
    const char* _constellationPath;
    const char* _sentinelName;
    const char* _packageName;
    const char* _accessKey;
    const char* _msgSubscriptionId;
    const char* _soSubscriptionId;
    const char* _base64Authorization;
    const char* _userAgent = DEFAULT_HTTP_USERAGENT;
    uint16_t _httpTimeout = DEFAULT_REQUEST_TIMEOUT;
    uint8_t _debugMode = (uint8_t)Info;
    void (*_msgCallback)(JsonObject&);
    void (*_msgCallbackWithContext)(JsonObject&, MessageContext);
    void (*_soCallback)(JsonObject&);
    bool (*_onClientConnected)(TNetworkClass&);
    typedef struct {
        MessageCallbackDescriptor descriptor;
        MESSAGE_CALLBACK_SIGNATURE;
        MESSAGE_CALLBACK_WCONTEXT_SIGNATURE;
        const char* id;
        bool isSagaCallback;
    } MessageCallbackSubscription;
    typedef struct {
        STATEOBJECT_CALLBACK_SIGNATURE;
        const char* sentinel;
        const char*    package;
        const char*    name;
        const char*    type;
    } StateObjectSubscription;
    typedef struct {
        TypeDescriptor descriptor;
        const char* name;
        DescriptorType type;
    } TypeDescriptorItem;
    LinkedList<MessageCallbackSubscription> _msgCallbacks = LinkedList<MessageCallbackSubscription>();
    LinkedList<StateObjectSubscription> _soCallbacks = LinkedList<StateObjectSubscription>();
    LinkedList<TypeDescriptorItem> _typeDescriptors = LinkedList<TypeDescriptorItem>();
    LinkedList<const char*> _msgGroups = LinkedList<const char*>();
    
    void addTypeDescriptor(const char* typeName, DescriptorType descriptorType, TypeDescriptor typeDescriptor) {
        TypeDescriptorItem type;
        type.type = descriptorType;
        type.name = typeName;
        type.descriptor = typeDescriptor;
        _typeDescriptors.add(type);
    };
    bool registerMessageCallback(const char* id, bool isSagaCallback, MessageCallbackDescriptor descriptor, MESSAGE_CALLBACK_SIGNATURE, MESSAGE_CALLBACK_WCONTEXT_SIGNATURE) {
        if(subscribeToMessage()) {
            MessageCallbackSubscription mc;
            mc.id = id;
            mc.isSagaCallback = isSagaCallback;
            mc.msgCallback = msgCallback;
            mc.msgCallbackWithContext = msgCallbackWithContext;
            mc.descriptor = descriptor;
            _msgCallbacks.add(mc);
            return true;
        }
        else {
            return false;
        }
    };
    const char* getLevelLabel(LogLevel level) {
        switch(level) {
            case LevelError:
                return "Error";
            case LevelWarn:
                return "Warn";
            default:
                return "Info";
        }
    };
    const char* getScopeLabel(ScopeType scope) {
        switch(scope) {
            case Group:
                return "Group";
            case Package:
                return "Package";
            case Sentinel:
                return "Sentinel";
            case Other:
                return "Others";
            case All:
                return "All";
            default:
                return "None";
        }
    };
    String createUri(const char* method, const char * args[], int argsSize) {
        String url = String(this->_constellationPath) + method;
        if(args != NULL) {
            for (int i = 0; i + 1 < argsSize * 2; i+=2){
                url += (i == 0) ? "?" : "&";
                url += args[i];
                url += "=";
                url += urlEncode(args[i + 1]);
            }
        }
        return url;
    };    
    int sendPostRequest(const char* method, JsonObject& content, String* response) {
        // Send request
        if (!_netClient.connected() && !_netClient.connect(this->_constellationHost, this->_constellationPort)) {
            log_error("Unable to establish the TCP connection to %s:%d (POST on %s)", this->_constellationHost, this->_constellationPort, method);
            return false;
        }
        // Verify the client connection
        if(this->_onClientConnected && !this->_onClientConnected(_netClient)) {
            log_error("Unable to verify the network client connection");            
            return false;
        }
        // We now create a URI for the request
        String url = createUri(method, NULL, 0);
        // Prepare the request        
        static BufferedPrint<NETCLIENT_BUFFER_SIZE> buffer(_netClient);
        log_debug("POST: %s", url.c_str());
        buffer.setDebug((this->_debugMode >= (int8_t)Trace));
        int contentLength = content.measureLength();
        String request = "POST " + url + " HTTP/1.1\r\n" +
                        "Host: " + this->_constellationHost + "\r\n" +                        
                        "SentinelName: " + this->_sentinelName + "\r\n"  +
                        "PackageName: " + this->_packageName + "\r\n"  +
                        "AccessKey: " + this->_accessKey + "\r\n"  +                        
                        "User-Agent: " + this->_userAgent + "\r\n"  +
                        "Accept-Encoding: identity\r\n" +
                        "Content-Length: " + contentLength + "\r\n"  +
                        "Content-Type: application/json\r\n" +
                        "Connection: keep-alive\r\n";
        if(this->_base64Authorization) {
            request += String("Authorization: Basic ") + this->_base64Authorization + "\r\n";
        }
        request += "\r\n";
        // This will send the request to the server
        buffer.print(request);
        content.printTo(buffer);
        buffer.print("\r\n\r\n");
        buffer.flush();
        // Read the response
        int statusCode = readResponse(&_netClient, response);
        if(statusCode >= 300) {
            log_error("Incorrect response: %d", statusCode);
            if(response != NULL) {
                log_debug(response->c_str());
            }
        }
        else {
            log_debug("Return code: %d", statusCode);
        }
        // Clean up
        //_netClient.stop();
        return statusCode;
    };    
    int sendRequest(const char* method, const char * args[], int argsSize, String* response) {
        // Send request
        if(!writeRequest(&_netClient, method, args, argsSize, true)) {
            log_error("Unable to send the request !");
            return false;
        }
        // Read the response
        int statusCode = readResponse(&_netClient, response);
         if(statusCode >= 300) {
            log_error("Incorrect response: %d", statusCode);
            if(response != NULL) {
                log_debug(response->c_str());
            }
        }
        else {
            log_debug("Return code: %d", statusCode);
        }
        // Clean up
        //_netClient.stop();
        return statusCode;
    };
    bool writeRequest(TNetworkClass* client, const char* method, const char * args[], int argsSize, bool keepAlive) {
        if (!client->connected() && !client->connect(this->_constellationHost, this->_constellationPort)) {
            log_error("Unable to establish the TCP connection to %s:%d (GET on %s)", this->_constellationHost, this->_constellationPort, method);
            return false;
        }
        // Verify the client connection
        if(this->_onClientConnected && !this->_onClientConnected(_netClient)) {
            log_error("Unable to verify the network client connection");            
            return false;
        }
        // We now create a URI for the request
        String url = createUri(method, args, argsSize);
        log_debug("GET: %s", url.c_str());
        // Prepare the request
        String request = "GET " + url + " HTTP/1.1\r\n" +
                        "Host: " + this->_constellationHost + "\r\n" +                        
                        "SentinelName: " + this->_sentinelName + "\r\n"  +
                        "PackageName: " + this->_packageName + "\r\n"  +
                        "AccessKey: " + this->_accessKey + "\r\n"  +                        
                        "User-Agent: " + this->_userAgent + "\r\n" +
                        "Accept-Encoding: identity\r\n" +
                        "Connection: " + (keepAlive ? "keep-alive" : "close") + "\r\n";
        if(this->_base64Authorization) {
            request += String("Authorization: Basic ") + this->_base64Authorization + "\r\n";
        }
        request += "\r\n";
        // This will send the request to the server
        client->print(request);
        return true;
    };
    
    int readResponse(TNetworkClass* client, String* response) {
        int statusCode = 0;
        if (!client->connected()) {
            return statusCode;
        }
        // Wait for response
        unsigned long timeout = millis();
        while (client->available() == 0) {
            if ((millis() - timeout) > this->_httpTimeout) {
                log_error("HTTP Timeout reached");
                return 0;
            }
        }
        String line;
        bool isChunked = false;
        bool firstLine = true;
        bool isBody = false;
        // Read the response
        while (client->available()) {            
            if (!isBody) { // Read the header                 
                line = client->readStringUntil('\n');
                line.trim();
                log_trace("> %s", line.c_str());
                if (firstLine && line.length() > 0) { // first line
                    int spaceIndex = line.indexOf(' ');
                    statusCode = line.substring(spaceIndex + 1, spaceIndex + 4).toInt();
                    firstLine = false;
                }
                else if (!firstLine && line.equals("Transfer-Encoding: chunked")) {
                    isChunked = true;
                }
                else if (statusCode > 0 && line.length() == 0) { // End of the header
                    isBody = true;
                }
            }
            else {
                if (response == NULL) {// No response expected
                    break;
                }
                if (!isChunked) {
                    response->concat((char)client->read());
                }
                else {
                    while(true) {
                        if(!client->connected()) {
                            log_error("Connection lost while reading the chunked response");
                            break; 
                        }
                        line = client->readStringUntil('\n');
                        if(line.length() <= 0) {
                            break;
                        }
                        line.trim();
                        // read size of chunk
                        long chunckLength = strtol((const char *) line.c_str(), NULL, 16);
                        log_trace("chunckLength: %d", chunckLength);
                        // data left?
                        if(chunckLength > 0) {
                            for (long i = 0; client->available() && i < chunckLength; ++i) {
                                response->concat((char)client->read());
                            }
                        } else {
                             break;                           
                        }
                        // read trailing \r\n at the end of the chunk
                        char buf[2];
                        auto trailing_seq_len = client->readBytes((uint8_t*)buf, 2);
                        if (trailing_seq_len != 2 || buf[0] != '\r' || buf[1] != '\n') {
                            log_debug("Reading timeout");
                            break;
                        }
                        delay(0);
                    }
                }
            }
        }
        log_trace("HTTP response code: %d", statusCode);
        if(response != NULL) {
            log_debug("Raw message: %s", response->c_str());
        }
        return statusCode;
    };    
    String urlEncode(const char* msg) {
        // Unreserved Characters = ALPHA / DIGIT / "-" / "." / "_" / "~"
        // http://www.ietf.org/rfc/rfc3986.txt
        const char *hex = "0123456789abcdef"; 
        String encodedMsg = "";
        while (*msg!='\0'){
            if( ('a' <= *msg && *msg <= 'z')
                    || ('A' <= *msg && *msg <= 'Z')
                    || ('0' <= *msg && *msg <= '9') 
                    || *msg == '-' || *msg == '.' || *msg == '_' || *msg == '~' || *msg == '*' ) {
                encodedMsg += *msg;
            } else {
                encodedMsg += '%';
                encodedMsg += hex[*msg >> 4];
                encodedMsg += hex[*msg & 15];
            }
            msg++;
        }
        return encodedMsg;
    };

    const char* stringFormat(const char* format, va_list myargs) {
        static char result[STRING_FORMAT_BUFFER];
        vsnprintf(result, STRING_FORMAT_BUFFER, format, myargs);
        return result;
    };
    const char* stringFormat(const char* format, ...) {
        static char result[STRING_FORMAT_BUFFER];
        va_list myargs;
        va_start(myargs, format);
        vsnprintf(result, STRING_FORMAT_BUFFER, format, myargs);
        va_end(myargs);
        return result;
    };

    void log(const char* message, va_list myargs, DebugMode level) {
        if (Serial && this->_debugMode >= (int8_t)level) {
            switch(level) {
                case 1:
                    Serial.print("[ERROR] ");
                    break;
                case 2:
                    Serial.print("[INFO] ");
                    break;
                case 3:
                    Serial.print("[DEBUG] ");
                    break;
                case 4:
                    Serial.print("[TRACE] ");
                    break;
                default:
                    break;
            }            
            static char internal_log[STRING_FORMAT_BUFFER];
            vsnprintf(internal_log, STRING_FORMAT_BUFFER, message, myargs);
            Serial.println(internal_log);
        }
    }
    void log_error(const char* message, ...) {
        if (this->_debugMode >= (int8_t)Error && Serial) {
            va_list myargs;
            va_start(myargs, message);
            log(message, myargs, Error);
            va_end(myargs);
        }
    }
    void log_info(const char* message, ...) {
        if (this->_debugMode >= (int8_t)Info && Serial) {
            va_list myargs;
            va_start(myargs, message);
            log(message, myargs, Info);
            va_end(myargs);
        }
    }
    void log_debug(const char* message, ...) {
        if (this->_debugMode >= (int8_t)Debug && Serial) {
            va_list myargs;
            va_start(myargs, message);
            log(message, myargs, Debug);
            va_end(myargs);
        }
    }
    void log_trace(const char* message, ...) {
        if (this->_debugMode >= (int8_t)Trace && Serial) {
            va_list myargs;
            va_start(myargs, message);
            log(message, myargs, Trace);
            va_end(myargs);
        }
    }

    void renewSubscriptions() {
        log_debug("Renew the message subscription");
        if(!subscribeToMessage(true)) {
            log_error("Unable to renew the message subscription");
            return;
        }
        if(_msgGroups.size() > 0) {
            for(int i = 0; i < _msgGroups.size(); i++) {
                const char* group = _msgGroups.get(i);
                log_debug("Renew subscription for the group %s", group);
                if(!subscribeToGroup(group, true)) {
                    log_error("Unable to renew the subscription for the group %s", group);
                    return;
                }
            }
         }
        if(_soCallbacks.size() > 0) {
            for(int i = 0; i < _soCallbacks.size(); i++) {
                StateObjectSubscription subcription = _soCallbacks.get(i);
                log_debug("Renew the subscription for the StateObjects %s/%s/%s/%s", subcription.sentinel, subcription.package, subcription.name, subcription.type);
                if(!subscribeToStateObjects(subcription.sentinel, subcription.package, subcription.name, subcription.type)) {
                    log_error("Unable to renew the subscription for the StateObjects %s/%s/%s/%s", subcription.sentinel, subcription.package, subcription.name, subcription.type);
                    return;
                }
            }
         }
    }

  public:
    Constellation() { };
    Constellation(const char * constellationHost, uint16_t constellationPort) {
        setServer(constellationHost, constellationPort);
    };
    Constellation(const char * constellationHost, uint16_t constellationPort, const char * sentinel, const char * package, const char * accessKey) {
        setServer(constellationHost, constellationPort);
        setIdentity(sentinel, package, accessKey);
    };
    Constellation(const char * constellationHost, uint16_t constellationPort, const char * path, const char * sentinel, const char * package, const char * accessKey) {
        setServer(constellationHost, constellationPort, path);
        setIdentity(sentinel, package, accessKey);
    };

    const char* getSentinelName() {
        return _sentinelName;
    };
    const char* getPackageName() {
        return _packageName;
    };
    
    void loop() {
        loop(DEFAULT_SUBSCRIPTION_TIMEOUT, DEFAULT_SUBSCRIPTION_LIMIT);
    };
    void loop(int timeout) {
        loop(timeout, DEFAULT_SUBSCRIPTION_LIMIT);
    };
    void loop(int timeout, int limit) {
        checkIncomingMessage(timeout, limit);
        checkStateObjectUpdate(timeout, limit);
    };
    void checkIncomingMessage() {
        checkIncomingMessage(DEFAULT_SUBSCRIPTION_TIMEOUT, DEFAULT_SUBSCRIPTION_LIMIT);
    };
    void checkIncomingMessage(int timeout) {
        checkIncomingMessage(timeout, DEFAULT_SUBSCRIPTION_LIMIT);
    };
    void checkIncomingMessage(int timeout, int limit) {
        if(this->_msgSubscriptionId != NULL) {
            if(_netClientMsg.connected() && !_netClientMsg.available()) {
                return;
            }
            else if (_netClientMsg.available()) {
                String response = "";
                // Read the response
                int statusCode = readResponse(&_netClientMsg, &response);
                if(statusCode == HTTP_OK) {
                    StaticJsonBuffer<JSON_PARSER_BUFFER_SIZE> jsonBuffer;
                    JsonArray& array = jsonBuffer.parseArray(response);
                    if (array.success()) {
                        if(_msgCallback || _msgCallbackWithContext || _msgCallbacks.size() > 0) {
                            for(int i=0; i < array.size(); i++) {
                                MessageContext ctx;
                                ctx.messageKey = array[i]["Key"].as<char *>(); 
                                ctx.sagaId = array[i]["Scope"]["SagaId"].as<char *>();
                                ctx.scope = (ScopeType)array[i]["Scope"]["Scope"].as<uint8_t>();
                                ctx.isSaga = ctx.sagaId != NULL;
                                ctx.sender.type = (SenderType)array[i]["Sender"]["Type"].as<uint8_t>();
                                ctx.sender.friendlyName = array[i]["Sender"]["FriendlyName"].as<char *>();
                                ctx.sender.connectionId = array[i]["Sender"]["ConnectionId"].as<char *>();
                                log_debug("Receiving message %s from %s", ctx.messageKey, ctx.sender.friendlyName);
                                if(_msgCallback) {
                                    log_debug("Invoking MessageReceiveCallback registered without context");
                                    _msgCallback(array[i]);
                                }
                                if(_msgCallbackWithContext) {
                                    log_debug("Invoking MessageReceiveCallback registered with context");
                                    _msgCallbackWithContext(array[i], ctx);
                                }
                                for(int j = 0; j < _msgCallbacks.size(); j++) {
                                    MessageCallbackSubscription mc = _msgCallbacks.get(j);
                                    if (mc.id && (mc.msgCallback || mc.msgCallbackWithContext) &&
                                        (mc.isSagaCallback ? ctx.sagaId != NULL : true) &&
                                        strcmp (mc.isSagaCallback ? ctx.sagaId : ctx.messageKey, mc.id) == 0) {
                                        if(mc.msgCallback) {
                                            log_debug("Invoking MessageCallback '%s' without context", mc.id);
                                            mc.msgCallback(array[i]);
                                        }
                                        if(mc.msgCallbackWithContext) {
                                            log_debug("Invoking MessageCallback '%s' with context", mc.id);
                                            mc.msgCallbackWithContext(array[i], ctx);
                                        }
                                        if(mc.isSagaCallback) {
                                            // remove the saga callback
                                            delete mc.id;
                                            _msgCallbacks.remove(j);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else {
                        log_error("Unable to parse the incoming message");
                    }
                }
                else if(statusCode == HTTP_SERVER_ERROR) {
                    log_error("Unable to get messages : internal server error");
                    renewSubscriptions();
                }
            }
            // Do request
            String strTimeout = String(timeout);
            String strLimit = String(limit);
            const char* args[] = { "subscriptionId", this->_msgSubscriptionId,  "timeout", strTimeout.c_str(), "limit", strLimit.c_str() };
            writeRequest(&_netClientMsg, "GetMessages", args, 3, true);
        }
        else {
            log_trace("checkIncomingMessage : no SubcriptionId");
        }
    };

    void checkStateObjectUpdate() {
        checkStateObjectUpdate(DEFAULT_SUBSCRIPTION_TIMEOUT, DEFAULT_SUBSCRIPTION_LIMIT);
    };
    void checkStateObjectUpdate(int timeout) {
        checkStateObjectUpdate(timeout, DEFAULT_SUBSCRIPTION_LIMIT);
    };
    void checkStateObjectUpdate(int timeout, int limit) {
        if(this->_soSubscriptionId != NULL) {
            if(_netClientSO.connected() && !_netClientSO.available()) {
                return;
            }
            else if (_netClientSO.available()) {
                String response = "";
                // Read the response
                int statusCode = readResponse(&_netClientSO, &response);
                if(statusCode == HTTP_OK) {
                    StaticJsonBuffer<JSON_PARSER_BUFFER_SIZE> jsonBuffer;
                    JsonArray& array = jsonBuffer.parseArray(response);
                    if (array.success()) {
                        if(_soCallback || _soCallbacks.size() > 0) {
                            for(int i = 0; i < array.size(); i++) {
                                if(_soCallback) {
                                    log_debug("Invoking StateObject Callback");
                                    _soCallback(array[i]["StateObject"]);
                                }
                                if(_soCallbacks.size() > 0) {
                                    const char * sentinel = array[i]["StateObject"]["SentinelName"].as<char *>(); 
                                    const char * package = array[i]["StateObject"]["PackageName"].as<char *>();
                                    const char * name = array[i]["StateObject"]["Name"].as<char *>();
                                    const char * type = array[i]["StateObject"]["Type"].as<char *>();
                                    for(int j = 0; j < _soCallbacks.size(); j++) {
                                        StateObjectSubscription subcription = _soCallbacks.get(j);
                                        if( (strcmp (sentinel, subcription.sentinel) == 0 || strcmp (WILDCARD, subcription.sentinel) == 0) &&
                                            (strcmp (package, subcription.package) == 0 || strcmp (WILDCARD, subcription.package) == 0) &&
                                            (strcmp (name, subcription.name) == 0 || strcmp (WILDCARD, subcription.name) == 0) &&
                                            (strcmp (type, subcription.type) == 0 || strcmp (WILDCARD, subcription.type) == 0)) {
                                            log_debug("Invoking StateObjectLink registered for %s/%s/%s/%s", subcription.sentinel, subcription.package, subcription.name, subcription.type);
                                            subcription.soCallback(array[i]["StateObject"]);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else {
                        log_error("Unable to parse the StateObjects array");
                    }
                }
                else if(statusCode == HTTP_SERVER_ERROR) {
                    log_error("Unable to get StateObjectLinks : internal server error");
                    renewSubscriptions();
                }
            }
            // Do request
            String strTimeout = String(timeout);
            String strLimit = String(limit);
            const char* args[] = { "subscriptionId", this->_soSubscriptionId,  "timeout", strTimeout.c_str(), "limit", strLimit.c_str() };
            writeRequest(&_netClientSO, "GetStateObjects", args, 3, true);
        }
        else {
            log_trace("checkStateObjectUpdate : no SubcriptionId");
        }
    };

    bool subscribeToMessage(bool renew = false) {
        if(this->_msgSubscriptionId == NULL) {
            String response = "";
            if(sendRequest("SubscribeToMessage", NULL, 0, &response) == HTTP_OK && response.length() == SUBSCRIPTIONID_SIZE + 2) {
                static char subId[SUBSCRIPTIONID_SIZE + 1];
                response.substring(1, SUBSCRIPTIONID_SIZE + 2).toCharArray(subId, sizeof(subId));
                this->_msgSubscriptionId = subId;
                log_info("SubscribeToMessage:OK - Subscription Id = %s", this->_msgSubscriptionId);
            }
            else {
                log_error("Unable to SubscribeToMessage");
            }
        }
        else if(renew) {
            const char* args[] = { "subscriptionId", this->_msgSubscriptionId };
            return sendRequest("SubscribeToMessage", args, 1, NULL) == HTTP_OK;
        }
        return this->_msgSubscriptionId != NULL;
    };
    bool subscribeToGroup(const char* groupName, bool renew = false) {
        if(subscribeToMessage()) {
            if(!renew) {
                _msgGroups.add(groupName);
            }
            const char* args[] = { "subscriptionId", this->_msgSubscriptionId, "group", groupName };
            return sendRequest("SubscribeToMessageGroup", args, 2, NULL) == HTTP_OK;
        }
        else {
            return false;
        }
    };
    
    bool registerMessageCallback(const char* messageKey, MESSAGE_CALLBACK_SIGNATURE) {
        return registerMessageCallback(messageKey, false, MessageCallbackDescriptor().setHidden(), msgCallback, NULL);
    };
    bool registerMessageCallback(const char* messageKey, MessageCallbackDescriptor descriptor, MESSAGE_CALLBACK_SIGNATURE) {
        return registerMessageCallback(messageKey, false, descriptor, msgCallback, NULL);
    };
    bool registerMessageCallback(const char* messageKey, MESSAGE_CALLBACK_WCONTEXT_SIGNATURE) {
        return registerMessageCallback(messageKey, false, MessageCallbackDescriptor().setHidden(), NULL, msgCallbackWithContext);
    };
    bool registerMessageCallback(const char* messageKey, MessageCallbackDescriptor descriptor, MESSAGE_CALLBACK_WCONTEXT_SIGNATURE) {
        return registerMessageCallback(messageKey, false, descriptor, NULL, msgCallbackWithContext);
    };

    void addMessageCallbackType(const char* typeName, TypeDescriptor typeDescriptor) {
        addTypeDescriptor(typeName, MessageCallbackType, typeDescriptor);
    };
    void addStateObjectType(const char* typeName, TypeDescriptor typeDescriptor) {
        addTypeDescriptor(typeName, StateObjectType, typeDescriptor);
    };
    
    bool declarePackageDescriptor() {
        DynamicJsonBuffer jsonBuffer;
        JsonObject& packageDescriptor = jsonBuffer.createObject();
        packageDescriptor["PackageName"] = this->_packageName;

        JsonArray& mcObj = packageDescriptor.createNestedArray("MessageCallbacks");
        JsonArray& mcTypes = packageDescriptor.createNestedArray("MessageCallbackTypes");
        JsonArray& soTypes = packageDescriptor.createNestedArray("StateObjectTypes");

        for(int i = 0; i < _msgCallbacks.size(); i++) {
            MessageCallbackSubscription mc = _msgCallbacks.get(i);
            if(!mc.isSagaCallback) {
                if(mc.descriptor.isHidden()) {
                    log_debug("Skipping the hidden MessageCallback '%s' to the PackageDescriptor", mc.id);
                }
                else {
                    JsonObject& nestedObject = mcObj.createNestedObject();
                    nestedObject["MessageKey"] = mc.id;
                    mc.descriptor.fillJsonObject(nestedObject);
                }
            }
        }

        for(int i = 0; i < _typeDescriptors.size(); i++) {
            TypeDescriptorItem type = _typeDescriptors.get(i);
            JsonObject& nestedObject = (type.type == MessageCallbackType ? mcTypes : soTypes).createNestedObject();
            nestedObject["TypeName"] = type.name;
            nestedObject["TypeFullname"] = type.name;
            type.descriptor.fillJsonObject(nestedObject);
        }

        return sendPostRequest("DeclarePackageDescriptor", packageDescriptor, NULL) == HTTP_NO_CONTENT;
    };

    bool subscribeToStateObjects(const char * sentinel, const char * package) {
        return subscribeToStateObjects(sentinel, package, WILDCARD, WILDCARD);
    };
    bool subscribeToStateObjects(const char * sentinel, const char * package, const char * name) {
        return subscribeToStateObjects(sentinel, package, name, WILDCARD);
    };
    bool subscribeToStateObjects(const char * sentinel, const char * package, const char * name, const char * type) {
        if(this->_soSubscriptionId == NULL) {
            String response = "";
            const char* args[] = { "sentinel", sentinel, "package", package, "name", name, "type", type };    
            if(sendRequest("SubscribeToStateObjects", args, 4, &response) == HTTP_OK && response.length() == SUBSCRIPTIONID_SIZE + 2) {
                static char subId[SUBSCRIPTIONID_SIZE + 1];
                response.substring(1, SUBSCRIPTIONID_SIZE + 2).toCharArray(subId, sizeof(subId));
                this->_soSubscriptionId = subId;
                log_info("SubscribeToStateObjects:OK - Subscription Id = %s", this->_soSubscriptionId);
            }
            else if(response == "null") {
                log_error("Unable to SubscribeToStateObjects : check your credential !");
            }
            else {
                log_error("Unable to SubscribeToStateObjects");
            }
            return this->_soSubscriptionId != NULL;
        }
        else {
            const char* args[] = { "subscriptionId", this->_soSubscriptionId, "sentinel", sentinel, "package", package, "name", name, "type", type };
            return sendRequest("SubscribeToStateObjects", args, 5, NULL) == HTTP_OK;
        }
    };
    
    bool registerStateObjectLink(const char * sentinel, const char * package, STATEOBJECT_CALLBACK_SIGNATURE) {
        return registerStateObjectLink(sentinel, package, WILDCARD, WILDCARD, soCallback);
    };
    bool registerStateObjectLink(const char * sentinel, const char * package, const char * name, STATEOBJECT_CALLBACK_SIGNATURE) {
        return registerStateObjectLink(sentinel, package, name, WILDCARD, soCallback);
    };
    bool registerStateObjectLink(const char * sentinel, const char * package, const char * name, const char * type, STATEOBJECT_CALLBACK_SIGNATURE) {
        StateObjectSubscription subscription;
        subscription.sentinel = sentinel;
        subscription.package = package;
        subscription.name = name;
        subscription.type = type;
        subscription.soCallback = soCallback;
        _soCallbacks.add(subscription);
        return subscribeToStateObjects(sentinel, package, name, type);
    };

    JsonArray& requestStateObjects(const char * sentinel, const char * package) {
        return requestStateObjects(sentinel, package, WILDCARD, WILDCARD);
    };
    JsonArray& requestStateObjects(const char * sentinel, const char * package, const char * name) {
        return requestStateObjects(sentinel, package, name, WILDCARD);
    };
    JsonArray& requestStateObjects(const char * sentinel, const char * package, const char * name, const char * type) {
        String response = "";    
        const char* args[] = { "sentinel", sentinel, "package", package, "name", name, "type", type };
        if(sendRequest("RequestStateObjects", args, 4, &response) == HTTP_OK) {
            StaticJsonBuffer<JSON_PARSER_BUFFER_SIZE> jsonBuffer;
            JsonArray& obj = jsonBuffer.parseArray(response);
            if (obj.success()) {
                return obj;
            }
            else {
                log_error("Unable to parse the StateObjects array !");
            }
        }
        return JsonArray::invalid();
    };

    JsonObject& getSettings() {
        String response = "";
        if(sendRequest("GetSettings", NULL, 0, &response) == HTTP_OK) {
            StaticJsonBuffer<JSON_PARSER_BUFFER_SIZE> jsonBuffer;
            JsonObject& obj = jsonBuffer.parseObject(response);
            if (obj.success()) {
                return obj;
            }
            else {
                log_info("Unable to parse the settings object !");
            }
        }
        return JsonObject::invalid();
    };

    bool sendResponse(MessageContext context, const char* data, ...) {
        va_list myargs;
        va_start(myargs, data);
        const char* pData = stringFormat(data, myargs);
        va_end(myargs);
        return sendResponse(context, pData);
    };
    bool sendResponse(MessageContext context, JsonVariant data) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject& msg = jsonBuffer.createObject();
        msg["Key"] = "__Response";
        if(data.is<const char*>() || data.is<char*>()) {
            const char * strValue = data.as<const char*>();
             if (strValue[0] == '{' || strValue[0] == '[') {
                data = RawJson(strValue);
             }
        }
        msg["Data"] = data;
        JsonObject& scopeObject = msg.createNestedObject("Scope");
        scopeObject["SagaId"] = context.sagaId;
        scopeObject["Scope"] = (uint8_t)Package;
        JsonArray& scopeArgsArray = scopeObject.createNestedArray("Args");
        scopeArgsArray.add(context.sender.type == ConsumerHub ? context.sender.connectionId : context.sender.friendlyName);
        return sendPostRequest("SendMessage", msg, NULL) == HTTP_NO_CONTENT;
    };
    bool sendResponse(MessageContext context, JsonObject& data) {
        return sendResponse(context, &data);
    };

    bool sendMessage(ScopeType scope, const char* scopeArgs, const char* key, const char* data, ...) {
        va_list myargs;
        va_start(myargs, data);
        const char* msg = stringFormat(data, myargs);
        va_end(myargs);
        const char* args[] = { "scope", getScopeLabel(scope), "args", scopeArgs, "key", key, "data", msg };
        return sendRequest("SendMessage", args, 4, NULL) == HTTP_NO_CONTENT;
    };
    bool sendMessage(ScopeType scope, const char* scopeArgs, const char* key, JsonObject* data) {
        char strData [data->measureLength()];
        data->printTo(strData, sizeof(strData));
        return sendMessage(scope, scopeArgs, key, strData);
    };
    bool sendMessageWithSaga(MESSAGE_CALLBACK_SIGNATURE, ScopeType scope, const char* scopeArgs, const char* key, const char* data, ...) {
        va_list myargs;
        va_start(myargs, data);
        const char* msg = stringFormat(data, myargs);
        va_end(myargs);
        String *strSagaId = new String(millis());
        const char* sagaId = strSagaId->c_str();
        log_debug("SagaId: %s", sagaId);
        const char* args[] = { "scope", getScopeLabel(scope), "args", scopeArgs, "key", key, "data", msg, "sagaId", sagaId };
        if(sendRequest("SendMessage", args, 5, NULL) == HTTP_NO_CONTENT) {
            return registerMessageCallback(sagaId, true, MessageCallbackDescriptor().empty(), msgCallback, NULL);
        }
        else {
            return false;
        }
    };
    bool sendMessageWithSaga(MESSAGE_CALLBACK_SIGNATURE, ScopeType scope, const char* scopeArgs, const char* key, JsonObject* data) {
        char strData [data->measureLength()];
        data->printTo(strData, sizeof(strData));
        return sendMessageWithSaga(msgCallback, scope, scopeArgs, key, strData);
    };

    bool pushStateObject(const char* name, JsonVariant value, int lifetime = 0){
        return pushStateObject(name, value, NULL, NULL, lifetime);
    };
    bool pushStateObject(const char* name, JsonVariant value, JsonObject* metadatas, int lifetime = 0){
        return pushStateObject(name, value, NULL, metadatas, lifetime);
    };
    bool pushStateObject(const char* name, JsonVariant value, const char* type, int lifetime = 0){
        return pushStateObject(name, value, type, NULL, lifetime);
    };
    bool pushStateObject(const char* name, JsonVariant value, const char* type, JsonObject* metadatas, int lifetime = 0){
        DynamicJsonBuffer jsonBuffer;
        JsonObject& stateObject = jsonBuffer.createObject();
        stateObject["Name"] = name;
        bool isRawJson = false;
        if(value.is<const char*>() || value.is<char*>()) {
            const char * strValue = value.as<const char*>();
             if (strValue[0] == '{' || strValue[0] == '[') {
                value = RawJson(strValue);
                isRawJson = true;
             }
        }
        stateObject["Value"] = value;
        if(type == NULL) {
            if(value.is<bool>()) {
                stateObject["Type"] = "System.Boolean";
            }
            else if(value.is<float>()) {
                stateObject["Type"] = "System.Float";
            }
            else if(value.is<double>()) {
                stateObject["Type"] = "System.Double";
            }
            else if(value.is<signed int>() || value.is<unsigned int>()) {
                stateObject["Type"] = "System.Int32";
            }
            else if(value.is<signed short>() || value.is<unsigned short>()) {
                stateObject["Type"] = "System.Int16";
            }
            else if(value.is<signed long>() || value.is<unsigned long>()) {
                stateObject["Type"] = "System.Int64";
            }
            else if(!isRawJson && (value.is<const char*>() || value.is<char*>())) {
                stateObject["Type"] = "System.String";
            }
            else if(value.is<signed char>() || value.is<unsigned char>()) {
                stateObject["Type"] = "System.Char";
            }
        }
        else {
            stateObject["Type"] = type;	
        }
        if(lifetime > 0) {
            stateObject["Lifetime"] = lifetime;
        }
        if(metadatas != NULL) {
            stateObject["Metadatas"] = *metadatas;
        }
        return sendPostRequest("PushStateObject", stateObject, NULL) == HTTP_NO_CONTENT;
    };

    bool purgeStateObjects() {
        return pushStateObject(WILDCARD, WILDCARD);
    };
    bool purgeStateObjects(const char* name) {
        return pushStateObject(name, WILDCARD);
    };
    bool purgeStateObjects(const char* name, const char* type) {
        const char* args[] = { "name", name, "type", type };
        return sendRequest("PurgeStateObjects", args, 2, NULL) == HTTP_NO_CONTENT;
    };

    bool writeInfo(const char* text, ...) {
        va_list myargs;
        va_start(myargs, text);
        const char* msg = stringFormat(text, myargs);
        va_end(myargs);
        return writeLog(msg, LevelInfo);
    };
    bool writeWarn(const char* text, ...) {
        va_list myargs;
        va_start(myargs, text);
        const char* msg = stringFormat(text, myargs);
        va_end(myargs);
        return writeLog(msg, LevelWarn);
    };
    bool writeError(const char* text, ...) {
        va_list myargs;
        va_start(myargs, text);
        const char* msg = stringFormat(text, myargs);
        va_end(myargs);
        return writeLog(msg, LevelError);
    };
    bool writeLog(const char* text, LogLevel level) {
        const char* args[] = { "message", text, "level", getLevelLabel(level) };
        log_info("WriteLog(%s) : %s", args[3], text);
        return sendRequest("WriteLog", args, 2, NULL) == HTTP_NO_CONTENT;
    };

    Constellation& setServer(const char * constellationHost, uint16_t constellationPort){
        return setServer(constellationHost, constellationPort, "/");
    };
    Constellation& setServer(const char * constellationHost, uint16_t constellationPort, const char * path){
        this->_constellationHost = constellationHost;
        this->_constellationPort = constellationPort;
        // Generate the base URI path
        static String strPath;
        strPath = String(path);
        if(!strPath.startsWith("/")) {
            strPath = "/" + strPath;
        }
        if(!strPath.endsWith("/")) {
            strPath += "/";
        }
        strPath += "rest/constellation/";
        this->_constellationPath = strPath.c_str();
        return *this;
    };
    Constellation& setIdentity(const char * sentinel, const char * package, const char * accessKey){
        this->_sentinelName = sentinel;
        this->_packageName = package;
        this->_accessKey = accessKey;
        return *this;
    };
    Constellation& setMessageReceiveCallback(MESSAGE_CALLBACK_SIGNATURE){
        this->_msgCallback = msgCallback;
        return *this;
    };
    Constellation& setMessageReceiveCallback(MESSAGE_CALLBACK_WCONTEXT_SIGNATURE){
        this->_msgCallbackWithContext = msgCallbackWithContext;
        return *this;
    };
    Constellation& setStateObjectUpdateCallback(STATEOBJECT_CALLBACK_SIGNATURE){
        this->_soCallback = soCallback;
        return *this;
    };
    Constellation& setDebugMode(bool activate){
        return setDebugMode(activate ? Debug : Info);
    };
    Constellation& setDebugMode(DebugMode mode){
        this->_debugMode = (uint8_t)mode;
        return *this;
    };
    Constellation& setAuthorization(const char * user, const char * password) {
        if(user && password) {
            String auth = user;
            auth += ":";
            auth += password;
            this->_base64Authorization = base64::encode(auth).c_str();
        }
        return *this;
    };
    Constellation& setAuthorization(const char * auth) {
        if(auth) {
            this->_base64Authorization = auth;
        }
        return *this;
    }; 
    Constellation& setUserAgent(const char * userAgent) {
        if(userAgent) {
            this->_userAgent = userAgent;
        }
        return *this;
    }; 
    Constellation& setTimeout(uint16_t timeout) {
        this->_httpTimeout = timeout;
        return *this;
    };
    Constellation& onClientConnected(bool (*onClientConnected)(TNetworkClass&)) {
        this->_onClientConnected = onClientConnected;
        return *this;
    };   
};

const char* stringFormat(const char* format, ...) {
    static char result[STRING_FORMAT_BUFFER];
    va_list myargs;
    va_start(myargs, format);
    vsnprintf(result, STRING_FORMAT_BUFFER, format, myargs);
    va_end(myargs);
    return result;
};

#endif
