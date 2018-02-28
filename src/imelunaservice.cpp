/* @@@LICENSE
*
*      Copyright (c) 2013 LG Electronics, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#include <QKeyEvent>
#include "imelunaservice.h"
#include "minputcontextconnection.h"
#include "luna-service2/lunaservice.h"

const char *IMELunaService::ServiceName = "com.webos.service.ime";
const char *IMELunaService::SubscriberKey = "REMOTE_KEYBOARD_LIST";

#include <QJsonObject>

namespace {

// RAII-style helper that automatically cleans up LSError
class LSErrorWrapper
{
public:
    LSErrorWrapper() { LSErrorInit(&err); }
    ~LSErrorWrapper() { LSErrorFree(&err); }
    operator LSError *() { return &err; }
    QString message() const { return QString(err.message ? err.message : ""); }

    LSError err;
};

class LSMessageAdapter
{
public:
    LSMessageAdapter(LSMessage *message)
        : m_message(message)
    {
    }

    bool isSubscription() const
    {
        return LSMessageIsSubscription(m_message);
    }

    void addSubscription(const char *key)
    {
        LSErrorWrapper err;

        if (!LSSubscriptionAdd(LSMessageGetConnection(m_message), key, m_message, err)) {
            qWarning() << "failed to add subscription: " << err.message();
            return;
        }
    }

    QString uniqueToken()
    {
        return QString(LSMessageGetUniqueToken(m_message));
    }

    QJsonObject getPayload() const
    {
        QByteArray data(LSMessageGetPayload(m_message));
        return QJsonDocument::fromJson(data).object();
    }

    void respond(const QJsonObject &response)
    {
        LSErrorWrapper err;

        QJsonDocument document(response);

        if (!LSMessageRespond(m_message, document.toJson().constData(), err)) {
            qWarning() << "failed to reply to LS2 message";
        }
    }

    void replyTrue()
    {
        QJsonObject response;
        response.insert("returnValue", true);
        respond(response);
    }

    void replyError(const QString &errorText = "", int errorCode = -1000)
    {
        QJsonObject response;
        response.insert("returnValue", false);
        response.insert("errorCode", errorCode);
        response.insert("errorText", errorText);

        respond(response);
    }

protected:
    LSMessage *m_message;
};

} // namespace

IMELunaService::IMELunaService(QSharedPointer<MInputContextConnection> connection)
    : m_connection(connection)
    , m_mainLoop(NULL)
    , m_handle(NULL)
    , m_focusChangedSinceLastBroadcast(false)
    , m_broadcastTimer(new QTimer(this))
{
    startService();

    connect(m_connection.data(), &MInputContextConnection::widgetStateChanged, this, &IMELunaService::onWidgetStateChanged);
    connect(m_connection.data(), &MInputContextConnection::resetInputMethodRequest, this, &IMELunaService::onReset);
    connect(m_broadcastTimer, &QTimer::timeout, this, &IMELunaService::broadcastWidgetState);
}

IMELunaService::~IMELunaService()
{
    if (m_mainLoop) {
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = NULL;
    }

    if (m_handle) {
        LSErrorWrapper err;

        LSUnregister(m_handle, err);
    }
}

bool IMELunaService::hasSubscribers() const
{
    return !m_clientByToken.isEmpty();
}

void IMELunaService::broadcastToSubscribers(QJsonObject response)
{
    LSErrorWrapper err;
    QJsonDocument document(response);

    if (!LSSubscriptionReply(m_handle, IMELunaService::SubscriberKey, document.toJson().constData(), err)) {
        qWarning() << "failed to reply to LS2 message";
    }
}

// Returns a JSON object representing the current input widget state
QJsonObject IMELunaService::getWidgetStateJson() const
{
    QJsonObject state;
    bool valid = false;

    bool focusState = m_connection->focusState(valid);

    if (valid) {
        state.insert("focus", focusState);
    }

    bool correctionEnabled = m_connection->correctionEnabled(valid);

    if (valid) {
        state.insert("correctionEnabled", correctionEnabled);
    }

    bool predictionEnabled = m_connection->predictionEnabled(valid);

    if (valid) {
        state.insert("predictionEnabled", predictionEnabled);
    }

    bool autoCapitalizationEnabled = m_connection->autoCapitalizationEnabled(valid);

    if (valid) {
        state.insert("autoCapitalizationEnabled", autoCapitalizationEnabled);
    }

    bool hiddenText = m_connection->hiddenText(valid);

    if (valid) {
        state.insert("hiddenText", hiddenText);
    }

    QString surroundingText;
    int cursorPosition = 0;

    // For security, only send minimal information about surrounding text
    if (m_connection->surroundingText(surroundingText, cursorPosition)) {
        state.insert("hasSurroundingText", !surroundingText.isEmpty());

        if (!hiddenText) {
            state.insert("cursorPosition", cursorPosition);
            state.insert("surroundingTextLength", surroundingText.length());
        }
    }

    bool hasSelection = m_connection->hasSelection(valid);

    if (valid && hasSelection) {
        state.insert("hasSelection", hasSelection);

        if (!hiddenText) {
            int anchorPosition = m_connection->anchorPosition(valid);

            if (valid) {
                state.insert("anchorPosition", anchorPosition);
            }
        }
    }

    int contentTypeInt = m_connection->contentType(valid);

    if (valid) {
        QString contentType;

        switch (contentTypeInt) {
        case Maliit::FreeTextContentType: contentType = "text"; break;
        case Maliit::NumberContentType: contentType = "number"; break;
        case Maliit::PhoneNumberContentType: contentType = "phonenumber"; break;
        case Maliit::EmailContentType: contentType = "email"; break;
        case Maliit::UrlContentType: contentType = "url"; break;
        default: contentType = "text"; break;
        }

        state.insert("contentType", contentType);
    }

    int enterKeyTypeInt = m_connection->enterKeyType(valid);

    if (valid) {
        state.insert("enterKeyType", enterKeyTypeInt);
    }

    return state;
}

void IMELunaService::onWidgetStateChanged(unsigned int clientId, const QMap<QString, QVariant> &newState,
                                         const QMap<QString, QVariant> &oldState, bool focusChanged)
{
    Q_UNUSED(clientId);
    Q_UNUSED(oldState);
    Q_UNUSED(newState);

    if (!m_handle) {
        return;
    }

    if (focusChanged) {
        m_focusChangedSinceLastBroadcast = true;
    }

    if (hasSubscribers()) {
        // run when event queue is empty (OK if already queued; QTimer will reschedule)
        m_broadcastTimer->setSingleShot(true);
        m_broadcastTimer->start(0);
    }
}

void IMELunaService::broadcastWidgetState()
{
    QJsonObject widgetState = getWidgetStateJson();

    QJsonObject response;
    response.insert("currentWidget", widgetState);
    response.insert("focusChanged", m_focusChangedSinceLastBroadcast);

    // Broadcast if focus or widget changed
    if (widgetState != m_lastWidgetState || m_focusChangedSinceLastBroadcast) {
        broadcastToSubscribers(response);
    }

    m_focusChangedSinceLastBroadcast = false;
    m_lastWidgetState = widgetState;
}

void IMELunaService::onReset()
{
    m_focusChangedSinceLastBroadcast = true;
    m_broadcastTimer->setSingleShot(true);
    m_broadcastTimer->start(0);
}

// Insert text at the current cursor position, replacing selected text (if any)
void IMELunaService::insertText(const QString& text, bool replace, ssize_t length)
{
    if (replace) {
        QString surroundingText;
        int cursorPos = 0;

        // Find out how much text to replace
        m_connection->surroundingText(surroundingText, cursorPos);

        if (length >= 0) {
            // Replace some text
            m_connection->sendCommitString(text, -length, length);
        } else {
            // Replace all text
            m_connection->sendCommitString(text, -cursorPos, surroundingText.length());
        }
    } else {
        // Insert text at current cursor position
        m_connection->sendCommitString(text);
    }
}

// Delete characters at the current cursor position, or all selected text (if any)
void IMELunaService::deleteCharacters(int numChars, DeleteMode mode)
{
    QString surroundingText;
    int cursorPos = 0;

    m_connection->surroundingText(surroundingText, cursorPos);

    if (mode == DirectMode) {
        // Generally less reliable in practice but more consistent
        numChars = std::min(numChars, cursorPos);
        m_connection->sendCommitString("", -numChars, numChars);
    } else {
        bool valid = false;
        bool hasSelection = m_connection->hasSelection(valid);

        if (hasSelection && valid) {
            // only hit delete once if there's a selection
            numChars = 1;
        }

        // Inject delete key presses
        // Ugly, but currently more reliable than sendCommitString since Maliit
        // doesn't seem to calculate the number of bytes to replace correctly yet

        for (int i = 0; i < numChars; i++) {
            m_connection->sendKeyEvent( QKeyEvent(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier) );
            m_connection->sendKeyEvent( QKeyEvent(QEvent::KeyRelease, Qt::Key_Backspace, Qt::NoModifier) );
        }
    }
}

void IMELunaService::sendEnterKey()
{
    QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    m_connection->sendKeyEvent(keyEvent);
}

extern "C" {

/*
 * Handler for LS2 service method palm://com.webos.service.ime/registerRemoteKeyboard
 *
 * Listens for changes to the input model. Bus client should remain subscribed as
 * long as it is actively using the keyboard.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/registerRemoteKeyboard '{"subscribe": true}'
 *
 * Parameters:
 *   subscribe - boolean (required). Must be true.
 *
 * Return payload:
 *   subscribed - boolean (required)
 *   returnValue - boolean (required)
 *   errorCode - boolean (optional)
 *   errorText - boolean (optional)
 *
 * Subscription update payload:
 *   focusChanged - boolean (required)
 *   currentWidget - object (optional)
 *     focus - boolean (optional)
 *     correctionEnabled - boolean (optional)
 *     predictionEnabled - boolean (optional)
 *     autoCapitalizationEnabled - boolean (optional)
 *     hiddenText - boolean (optional)
 *     hasSurroundingText - boolean (optional)
 *     surroundingTextLength - int (optional)
 *     cursorPosition - int (optional)
 *     hasSelection - boolean (optional)
 *     anchorPosition - int (optional)
 *     contentType - string (optional). One of "text", "number", "phonenumber", "email", "url"
 *     enterKeyType - int (optional). check enum EnterKeyType.
 */
bool IMELunaService::handleRegisterRemoteKeyboard(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);

    LSMessageAdapter msg(message);

    if (msg.isSubscription()) {
        msg.addSubscription(IMELunaService::SubscriberKey);

        // Track subscription
        QString token = msg.uniqueToken();

        QSharedPointer<RemoteKeyboardClient> client(new RemoteKeyboardClient());
        client->token = token;

        service->m_clientByToken.insert(token, client);

        qWarning() << "registering remote keyboard";

        // Send subscribe response with initial state
        QJsonObject response;
        response.insert("subscribed", true);

        QJsonObject state = service->getWidgetStateJson();
        if (!state.isEmpty()) {
            response.insert("currentWidget", state);
        }

        msg.respond(response);
    } else {
        QJsonObject response;

        response.insert("returnValue", false);
        response.insert("errorCode", -1000);
        response.insert("errorText", QString("Must subscribe to registerRemoteKeyboard"));

        msg.respond(response);
    }

    return true;
}

/*
 * Handler for LS2 service method palm://com.webos.service.ime/insertText
 *
 * Inserts text at the current insertion point, or replaces all text in the field.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/insertText '{"text": "hello world", "replace": true}'
 *
 * Parameters:
 *   text - string (required). Text to insert.
 *   replace - boolean (optional). If true, replace any existing text in field.
 *   replaceLength - number of characters to replace
 *
 * Return payload:
 *   returnValue - boolean (required)
 *   errorCode - boolean (optional)
 *   errorText - boolean (optional)
 */
bool IMELunaService::handleInsertText(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    QJsonObject payload = msg.getPayload();
    QJsonValue textParam = payload["text"];
    QJsonValue replaceParam = payload["replace"];
    QJsonValue replaceLengthParam = payload["replaceLength"];

    if (textParam.isString()) {
        bool replace = replaceParam.isBool() ? replaceParam.toBool() : false;
        ssize_t length = replaceLengthParam.isDouble() ?
                         ((int) replaceLengthParam.toDouble(-1)) : -1;

        service->insertText(textParam.toString(), replace, length);
        msg.replyTrue();
    } else {
        msg.replyError("Missing \"text\" parameter");
    }

    return true;
}

/*
 * Handler for LS2 service method palm://com.webos.service.ime/deleteCharacters
 *
 * Deletes characters from the current insertion point.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/deleteCharacters '{"count": 1}'
 *
 * Parameters:
 *   count - number (required). Number of characters to delete.
 *   mode - "backspace" (send backspace key)
 *          "direct" (remove characters directly from text string; single-line only)
 *
 * Return payload:
 *   returnValue - boolean (required)
 *   errorCode - boolean (optional)
 *   errorText - boolean (optional)
 */
bool IMELunaService::handleDeleteCharacters(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    QJsonObject payload = msg.getPayload();
    QJsonValue characterCount = payload["count"];
    QJsonValue modeParam = payload["mode"];

    DeleteMode mode = BackspaceMode;

    if (modeParam.isString()) {
        QString modeString = modeParam.toString();

        if (modeString == "backspace") {
            mode = BackspaceMode;
        } else if (modeString == "direct") {
            mode = DirectMode;
        } else {
            msg.replyError("Unknown \"mode\"; supported modes are \"backspace\" and \"direct\"");
            return true;
        }
    }

    if (characterCount.isDouble() && characterCount.toDouble() > 0) {
        service->deleteCharacters((int) characterCount.toDouble(), mode);
        msg.replyTrue();
    } else {
        msg.replyError("Missing or invalid \"count\" parameter");
    }

    return true;
}

/*
 * Handler for LS2 service method palm://com.webos.service.ime/sendEnterKey
 *
 * Sends the enter key in the current field.
 *
 * Example:
 *   luna-send -n 1 palm://com.webos.service.ime/sendEnterKey '{}'
 *
 * Parameters:
 *   no parameters
 *
 * Return payload:
 *   returnValue - boolean (required)
 *   errorCode - boolean (optional)
 *   errorText - boolean (optional)
 */
bool IMELunaService::handleSendEnterKey(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    service->sendEnterKey();

    msg.replyTrue();
    return true;
}

// Handle subscription cancellation
bool IMELunaService::handleSubscriptionCancel(LSHandle *handle, LSMessage *message, void *data)
{
    Q_UNUSED(handle);

    IMELunaService *service = static_cast<IMELunaService *>(data);
    LSMessageAdapter msg(message);

    service->m_clientByToken.remove(msg.uniqueToken());

    if (service->m_clientByToken.isEmpty()) {
        qWarning() << "all remote keyboard clients disconnected";

        // TODO: if all active clients have unsubscribed, do something (inform VKB?)
    }

    return true;
}

} // extern "C"

LSMethod IMELunaService::ime_bus_methods [] = {
    // Handlers for service methods for com.webos.service.ime
    {"registerRemoteKeyboard", IMELunaService::handleRegisterRemoteKeyboard, (LSMethodFlags) 0},
    {"insertText", IMELunaService::handleInsertText, (LSMethodFlags) 0},
    {"deleteCharacters", IMELunaService::handleDeleteCharacters, (LSMethodFlags) 0},
    {"sendEnterKey", IMELunaService::handleSendEnterKey, (LSMethodFlags) 0},

    {0, 0, (LSMethodFlags) 0}
};

void IMELunaService::startService()
{
    GMainContext *context = g_main_context_default();
    m_mainLoop = g_main_loop_new(context, TRUE);

    LSErrorWrapper err;

    if (!LSRegisterPubPriv(IMELunaService::ServiceName, &m_handle, false /* private bus */, err)) {
        qCritical() << "failed to register on bus: " << err.message();
        return;
    }

    if (!LSRegisterCategory(m_handle, "/", ime_bus_methods, NULL, NULL, err)) {
        qCritical() << "failed to register category on bus: " << err.message();
        return;
    }

    if (!LSCategorySetData(m_handle, "/", this, err)) {
        qCritical() << "failed to set category data: " << err.message();
        return;
    }

    if (!LSGmainAttach(m_handle, m_mainLoop, err)) {
        qCritical() << "unable to attach LS2 to main loop" << err.message();
        return;
    }

    if (!LSSubscriptionSetCancelFunction(m_handle, &IMELunaService::handleSubscriptionCancel, this, err)) {
        qCritical() << "error registering LS2 cancel function" << err.message();
        return;
    }

    qWarning() << IMELunaService::ServiceName << "LS2 service running";
}
