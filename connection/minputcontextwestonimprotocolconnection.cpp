/* * This file is part of Maliit framework *
 *
 * Copyright (C) 2012 Canonical Ltd
 *
 * Contact: maliit-discuss@lists.maliit.org
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * and appearing in the file LICENSE.LGPL included in the packaging
 * of this file.
 */

#include <cerrno> // for errno
#include <cstring> // for strerror
#include <unistd.h> // for close
#include <QGuiApplication>
#include <QKeyEvent>
#include <qweboskeyextension.h>
#include <qpa/qplatformnativeinterface.h>

#include "wayland-client.h"
#include "wayland-input-method-client-protocol.h"
#include "wayland-text-client-protocol.h"

#include <linux/input.h>
#ifdef HAS_LIBIM
#include <im_openapi_keycode.h>
#endif
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>

#include "minputcontextwestonimprotocolconnection.h"

namespace {

// TODO: Deduplicate it. Those values are used in
// minputcontextconnection, mimpluginmanager,
// mattributeextensionmanager and in input context implementations.
const char * const FocusStateAttribute = "focusState";
const char * const ContentTypeAttribute = "contentType";
const char * const EnterKeyTypeAttribute = "enterKeyType";
const char * const CorrectionAttribute = "correctionEnabled";
const char * const PredictionAttribute = "predictionEnabled";
const char * const AutoCapitalizationAttribute = "autocapitalizationEnabled";
const char * const SurroundingTextAttribute = "surroundingText";
const char * const AnchorPositionAttribute = "anchorPosition";
const char * const CursorPositionAttribute = "cursorPosition";
const char * const HasSelectionAttribute = "hasSelection";
const char * const HiddenTextAttribute = "hiddenText";
const char * const MaxTextLengthAttribute = "maxTextLength";
const char * const PlatformDataAttribute = "platformData";

struct XkbQtKey
{
    xkb_keysym_t xkbkey;
    int qtkey;
};

static const struct XkbQtKey g_XkbQtKeyMap[] = {
    { XKB_KEY_Shift_L,     Qt::Key_Shift },
    { XKB_KEY_Control_L,   Qt::Key_Control },
    { XKB_KEY_Super_L,     Qt::Key_Super_L },
    { Qt::Key_Super_L,     Qt::Key_Super_L },
    { XKB_KEY_Alt_L,       Qt::Key_Alt },

    { XKB_KEY_Shift_R,     Qt::Key_Shift },
    { XKB_KEY_Control_R,   Qt::Key_Control },
    { XKB_KEY_Super_R,     Qt::Key_Super_R },
    { XKB_KEY_Alt_R,       Qt::Key_Alt },

    { XKB_KEY_Hangul,      Qt::Key_Hangul},

    { XKB_KEY_Menu,        Qt::Key_Menu },
    { Qt::Key_Menu,        Qt::Key_Menu },
    { XKB_KEY_Escape,      Qt::Key_Escape },

    { XKB_KEY_space,       Qt::Key_Space },
    { XKB_KEY_BackSpace,   Qt::Key_Backspace },
    { XKB_KEY_Return,      Qt::Key_Return },
    { XKB_KEY_Tab,         Qt::Key_Tab },
    { XKB_KEY_ISO_Left_Tab,Qt::Key_Tab },
    { XKB_KEY_Caps_Lock,   Qt::Key_CapsLock },

    { XKB_KEY_F1,          Qt::Key_F1 },
    { XKB_KEY_F2,          Qt::Key_F2 },
    { XKB_KEY_F3,          Qt::Key_F3 },
    { XKB_KEY_F4,          Qt::Key_F4 },
    { XKB_KEY_F5,          Qt::Key_F5 },
    { XKB_KEY_F6,          Qt::Key_F6 },
    { XKB_KEY_F7,          Qt::Key_F7 },
    { XKB_KEY_F8,          Qt::Key_F8 },
    { XKB_KEY_F9,          Qt::Key_F9 },
    { XKB_KEY_F10,         Qt::Key_F10 },
    { XKB_KEY_F11,         Qt::Key_F11 },
    { XKB_KEY_F12,         Qt::Key_F12 },

    { XKB_KEY_Print,       Qt::Key_Print },
    { XKB_KEY_Pause,       Qt::Key_Pause },
    { XKB_KEY_Scroll_Lock, Qt::Key_ScrollLock },

    { XKB_KEY_Insert,      Qt::Key_Insert },
    { XKB_KEY_Delete,      Qt::Key_Delete },
    { XKB_KEY_Home,        Qt::Key_Home },
    { XKB_KEY_End,         Qt::Key_End },
    { XKB_KEY_Prior,       Qt::Key_PageUp },
    { XKB_KEY_Next,        Qt::Key_PageDown },

    { XKB_KEY_Up,          Qt::Key_Up },
    { XKB_KEY_Left,        Qt::Key_Left },
    { XKB_KEY_Down,        Qt::Key_Down },
    { XKB_KEY_Right,       Qt::Key_Right },

    { XKB_KEY_Num_Lock,    Qt::Key_NumLock },

    { XKB_KEY_A,           Qt::Key_A },
    { XKB_KEY_B,           Qt::Key_B },
    { XKB_KEY_C,           Qt::Key_C },
    { XKB_KEY_D,           Qt::Key_D },
    { XKB_KEY_E,           Qt::Key_E },
    { XKB_KEY_F,           Qt::Key_F },
    { XKB_KEY_G,           Qt::Key_G },
    { XKB_KEY_H,           Qt::Key_H },
    { XKB_KEY_I,           Qt::Key_I },
    { XKB_KEY_J,           Qt::Key_J },
    { XKB_KEY_K,           Qt::Key_K },
    { XKB_KEY_L,           Qt::Key_L },
    { XKB_KEY_M,           Qt::Key_M },
    { XKB_KEY_N,           Qt::Key_N },
    { XKB_KEY_O,           Qt::Key_O },
    { XKB_KEY_P,           Qt::Key_P },
    { XKB_KEY_Q,           Qt::Key_Q },
    { XKB_KEY_R,           Qt::Key_R },
    { XKB_KEY_S,           Qt::Key_S },
    { XKB_KEY_T,           Qt::Key_T },
    { XKB_KEY_U,           Qt::Key_U },
    { XKB_KEY_V,           Qt::Key_V },
    { XKB_KEY_W,           Qt::Key_W },
    { XKB_KEY_X,           Qt::Key_X },
    { XKB_KEY_Y,           Qt::Key_Y },
    { XKB_KEY_Z,           Qt::Key_Z },

    { XKB_KEY_a,           Qt::Key_A },
    { XKB_KEY_b,           Qt::Key_B },
    { XKB_KEY_c,           Qt::Key_C },
    { XKB_KEY_d,           Qt::Key_D },
    { XKB_KEY_e,           Qt::Key_E },
    { XKB_KEY_f,           Qt::Key_F },
    { XKB_KEY_g,           Qt::Key_G },
    { XKB_KEY_h,           Qt::Key_H },
    { XKB_KEY_i,           Qt::Key_I },
    { XKB_KEY_j,           Qt::Key_J },
    { XKB_KEY_k,           Qt::Key_K },
    { XKB_KEY_l,           Qt::Key_L },
    { XKB_KEY_m,           Qt::Key_M },
    { XKB_KEY_n,           Qt::Key_N },
    { XKB_KEY_o,           Qt::Key_O },
    { XKB_KEY_p,           Qt::Key_P },
    { XKB_KEY_q,           Qt::Key_Q },
    { XKB_KEY_r,           Qt::Key_R },
    { XKB_KEY_s,           Qt::Key_S },
    { XKB_KEY_t,           Qt::Key_T },
    { XKB_KEY_u,           Qt::Key_U },
    { XKB_KEY_v,           Qt::Key_V },
    { XKB_KEY_w,           Qt::Key_W },
    { XKB_KEY_x,           Qt::Key_X },
    { XKB_KEY_y,           Qt::Key_Y },
    { XKB_KEY_z,           Qt::Key_Z },

    { XKB_KEY_quoteleft,   Qt::Key_QuoteLeft },
    { XKB_KEY_asciitilde,  Qt::Key_AsciiTilde },

    { XKB_KEY_0,           Qt::Key_0 },
    { XKB_KEY_1,           Qt::Key_1 },
    { XKB_KEY_2,           Qt::Key_2 },
    { XKB_KEY_3,           Qt::Key_3 },
    { XKB_KEY_4,           Qt::Key_4 },
    { XKB_KEY_5,           Qt::Key_5 },
    { XKB_KEY_6,           Qt::Key_6 },
    { XKB_KEY_7,           Qt::Key_7 },
    { XKB_KEY_8,           Qt::Key_8 },
    { XKB_KEY_9,           Qt::Key_9 },

    { XKB_KEY_exclam,      Qt::Key_Exclam },
    { XKB_KEY_at,          Qt::Key_At },
    { XKB_KEY_numbersign,  Qt::Key_NumberSign },
    { XKB_KEY_dollar,      Qt::Key_Dollar },
    { XKB_KEY_percent,     Qt::Key_Percent },
    { XKB_KEY_asciicircum, Qt::Key_AsciiCircum },
    { XKB_KEY_ampersand,   Qt::Key_Ampersand },
    { XKB_KEY_asterisk,    Qt::Key_Asterisk },
    { XKB_KEY_parenleft,   Qt::Key_ParenLeft },
    { XKB_KEY_parenright,  Qt::Key_ParenRight },

    { XKB_KEY_plus,        Qt::Key_Plus },
    { XKB_KEY_minus,       Qt::Key_Minus },
    { XKB_KEY_equal,       Qt::Key_Equal },
    { XKB_KEY_underscore,  Qt::Key_Underscore },
    { XKB_KEY_bracketleft, Qt::Key_BracketLeft },
    { XKB_KEY_bracketright,Qt::Key_BracketRight },
    { XKB_KEY_braceleft,   Qt::Key_BraceLeft },
    { XKB_KEY_braceright,  Qt::Key_BraceRight },
    { XKB_KEY_backslash,   Qt::Key_Backslash },
    { XKB_KEY_bar,         Qt::Key_Bar },

    { XKB_KEY_colon,       Qt::Key_Colon },
    { XKB_KEY_semicolon,   Qt::Key_Semicolon },
    { XKB_KEY_quotedbl,    Qt::Key_QuoteDbl },
    { XKB_KEY_apostrophe,  Qt::Key_Apostrophe },

    { XKB_KEY_comma,       Qt::Key_Comma },
    { XKB_KEY_period,      Qt::Key_Period },
    { XKB_KEY_slash,       Qt::Key_Slash },
    { XKB_KEY_less,        Qt::Key_Less },
    { XKB_KEY_greater,     Qt::Key_Greater },
    { XKB_KEY_question,    Qt::Key_Question },

    { XKB_KEY_XF86Back,    Qt::Key_webOS_Back },
    { Qt::Key_webOS_Back,  Qt::Key_webOS_Back },
    { Qt::Key_webOS_Exit,  Qt::Key_webOS_Exit },
    { XKB_KEY_Cancel,      Qt::Key_MediaStop },
};

static const struct XkbQtKey g_XkbQtKeypadMap[] = {
    { XKB_KEY_KP_Divide,   Qt::Key_Slash },
    { XKB_KEY_KP_Multiply, Qt::Key_Asterisk },
    { XKB_KEY_KP_Subtract, Qt::Key_Minus },
    { XKB_KEY_KP_Add,      Qt::Key_Plus },

    { XKB_KEY_KP_Home,     Qt::Key_Home },
    { XKB_KEY_KP_Up,       Qt::Key_Up },
    { XKB_KEY_KP_Prior,    Qt::Key_PageUp },
    { XKB_KEY_KP_Left,     Qt::Key_Left },
    { XKB_KEY_KP_Begin,    Qt::Key_Clear },
    { XKB_KEY_KP_Right,    Qt::Key_Right },
    { XKB_KEY_KP_End,      Qt::Key_End },
    { XKB_KEY_KP_Down,     Qt::Key_Down },
    { XKB_KEY_KP_Next,     Qt::Key_PageDown },

    { XKB_KEY_KP_Insert,   Qt::Key_Insert },
    { XKB_KEY_KP_Delete,   Qt::Key_Delete },
    { XKB_KEY_KP_Enter,    Qt::Key_Enter },
    { XKB_KEY_KP_Decimal,  Qt::Key_Period },

    { XKB_KEY_KP_0,        Qt::Key_0 },
    { XKB_KEY_KP_1,        Qt::Key_1 },
    { XKB_KEY_KP_2,        Qt::Key_2 },
    { XKB_KEY_KP_3,        Qt::Key_3 },
    { XKB_KEY_KP_4,        Qt::Key_4 },
    { XKB_KEY_KP_5,        Qt::Key_5 },
    { XKB_KEY_KP_6,        Qt::Key_6 },
    { XKB_KEY_KP_7,        Qt::Key_7 },
    { XKB_KEY_KP_8,        Qt::Key_8 },
    { XKB_KEY_KP_9,        Qt::Key_9 },

    { Qt::Key_0,           Qt::Key_0 },
    { Qt::Key_1,           Qt::Key_1 },
    { Qt::Key_2,           Qt::Key_2 },
    { Qt::Key_3,           Qt::Key_3 },
    { Qt::Key_4,           Qt::Key_4 },
    { Qt::Key_5,           Qt::Key_5 },
    { Qt::Key_6,           Qt::Key_6 },
    { Qt::Key_7,           Qt::Key_7 },
    { Qt::Key_8,           Qt::Key_8 },
    { Qt::Key_9,           Qt::Key_9 },
};

static const struct XkbQtKey g_XkbQtMediaMap[] = {
    { XKB_KEY_XF86AudioPlay, Qt::Key_MediaPlay },
    { Qt::Key_MediaPlay,     Qt::Key_MediaPlay },
    { XKB_KEY_XF86AudioStop, Qt::Key_MediaStop },
    { Qt::Key_MediaStop,     Qt::Key_MediaStop },
    { XKB_KEY_XF86AudioPrev, Qt::Key_MediaPrevious },
    { Qt::Key_MediaPrevious, Qt::Key_MediaPrevious },
    { XKB_KEY_XF86AudioNext, Qt::Key_MediaNext },
    { Qt::Key_MediaNext,     Qt::Key_MediaNext },

    { XKB_KEY_XF86AudioRecord,  Qt::Key_MediaRecord },
    { Qt::Key_MediaRecord,      Qt::Key_MediaRecord },
    { XKB_KEY_XF86AudioRewind,  Qt::Key_AudioRewind },
    { Qt::Key_AudioRewind,      Qt::Key_AudioRewind },
    { XKB_KEY_XF86AudioForward, Qt::Key_AudioForward },
    { Qt::Key_AudioForward,     Qt::Key_AudioForward },
};

static xkb_keysym_t qtKeyToXkbKey(int qtkey)
{
    unsigned i;

    for (i = 0; i < sizeof(g_XkbQtKeyMap) / sizeof(XkbQtKey); i++) {
        if (qtkey == g_XkbQtKeyMap[i].qtkey)
            return g_XkbQtKeyMap[i].xkbkey;
    }
    for (i = 0; i < sizeof(g_XkbQtKeypadMap) / sizeof(XkbQtKey); i++) {
        if (qtkey == g_XkbQtKeypadMap[i].qtkey)
            return g_XkbQtKeypadMap[i].xkbkey;
    }
    for (i = 0; i < sizeof(g_XkbQtMediaMap) / sizeof(XkbQtKey); i++) {
        if (qtkey == g_XkbQtMediaMap[i].qtkey)
            return g_XkbQtMediaMap[i].xkbkey;
    }

    return (xkb_keysym_t)qtkey;
}

static int xkbKeyToQtKey(xkb_keysym_t xkbkey)
{
    unsigned i;

    for (i = 0; i < sizeof(g_XkbQtKeyMap) / sizeof(XkbQtKey); i++) {
        if (xkbkey == g_XkbQtKeyMap[i].xkbkey)
            return g_XkbQtKeyMap[i].qtkey;
    }
    for (i = 0; i < sizeof(g_XkbQtKeypadMap) / sizeof(XkbQtKey); i++) {
        if (xkbkey == g_XkbQtKeypadMap[i].xkbkey)
            return g_XkbQtKeypadMap[i].qtkey;
    }
    for (i = 0; i < sizeof(g_XkbQtMediaMap) / sizeof(XkbQtKey); i++) {
        if (xkbkey == g_XkbQtMediaMap[i].xkbkey)
            return g_XkbQtMediaMap[i].qtkey;
    }

    return (int)xkbkey;
}

static bool isKeypadKey(xkb_keysym_t xkbkey)
{
    unsigned i;
    for (i = 0; i < sizeof(g_XkbQtKeypadMap) / sizeof(XkbQtKey); i++) {
        if (xkbkey == g_XkbQtKeypadMap[i].xkbkey)
            return true;
    }
    return false;
}

struct ModTuple
{
    Qt::KeyboardModifier qt;
    const char *name;
    int size;
};

ModTuple tuples [] = {
    {Qt::ShiftModifier, XKB_MOD_NAME_SHIFT, sizeof(XKB_MOD_NAME_SHIFT)},
    {Qt::ControlModifier, XKB_MOD_NAME_CTRL, sizeof(XKB_MOD_NAME_CTRL)},
    {Qt::AltModifier, XKB_MOD_NAME_ALT, sizeof(XKB_MOD_NAME_ALT)},
    {Qt::MetaModifier, XKB_MOD_NAME_LOGO, sizeof(XKB_MOD_NAME_LOGO)},
    {Qt::KeypadModifier, XKB_LED_NAME_NUM, sizeof(XKB_LED_NAME_NUM)}
};

class Modifiers
{
public:
    Modifiers();
    ~Modifiers();

    xkb_mod_mask_t fromQt(const Qt::KeyboardModifiers& qt_mods) const;
    wl_array *getModMap();

private:
    wl_array mod_map;
};

Modifiers::Modifiers()
{
    wl_array_init(&mod_map);
    for (unsigned int iter(0); iter < (sizeof (tuples) / sizeof (tuples[0])); ++iter) {
        void *data(wl_array_add(&mod_map, tuples[iter].size));

        if (!data) {
            qCritical() << "Couldn't allocate memory in wl_array.";
            return;
        }

        memcpy(data, static_cast<const void *>(tuples[iter].name), tuples[iter].size);
    }
}

Modifiers::~Modifiers()
{
    wl_array_release (&mod_map);
}

xkb_mod_mask_t Modifiers::fromQt(const Qt::KeyboardModifiers& qt_mods) const
{
    xkb_mod_mask_t mod_mask(0);

    if (qt_mods != Qt::NoModifier) {
        for (unsigned int iter(0); iter < (sizeof (tuples) / sizeof (tuples[0])); ++iter) {
            if ((qt_mods & tuples[iter].qt) == tuples[iter].qt) {
                mod_mask |= 1 << iter;
            }
        }
    }

    return mod_mask;
}

// not const because I would be forced to use const_cast<> on mod_map
wl_array *Modifiers::getModMap()
{
    return &mod_map;
}

} // unnamed namespace

struct MInputContextWestonIMProtocolConnectionPrivate
{
    Q_DECLARE_PUBLIC(MInputContextWestonIMProtocolConnection);

    MInputContextWestonIMProtocolConnectionPrivate(MInputContextWestonIMProtocolConnection *connection);
    ~MInputContextWestonIMProtocolConnectionPrivate();

    void handleRegistryGlobal(uint32_t name,
                              const char *interface,
                              uint32_t version);
    void handleRegistryGlobalRemove(uint32_t name);
    void handleInputMethodActivate(input_method_context *context,
                                   uint32_t serial);
    void handleInputMethodDeactivate(input_method_context *context);
    void handleInputMethodContextSurroundingText(const char *text,
                                                 uint32_t cursor,
                                                 uint32_t anchor);
    void handleInputMethodContextReset(uint32_t serial);
    void handleInputMethodContextContentType(uint32_t hint,
                                             uint32_t purpose);
    void handleInputMethodContextEnterKeyType(uint32_t enter_key_type);

    void handleInputMethodContextInvokeAction(uint32_t button,
                                              uint32_t index);
    void handleInputMethodContextCommit();
    void handleInputMethodContextPreferredLanguage(const char *language);
    void handleInputMethodContextMaxTextLength(uint32_t maxLength);
    void handleInputMethodContextPlatformData(const char *pattern);

    void processKeyMap(uint32_t format, uint32_t fd, uint32_t size);
    void processKeyEvent(uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
    void processKeyModifiers(uint32_t serial, uint32_t mods_depressed, uint32_t
            mods_latched, uint32_t mods_locked, uint32_t group);

    MInputContextWestonIMProtocolConnection *q_ptr;
    wl_display *display;
    wl_registry *registry;
    input_method *im;
    input_method_context *im_context;
    uint32_t im_serial;
    QString selection;
    Modifiers mods;
    QMap<QString, QVariant> state_info;

    struct {
        xkb_context *context;
        xkb_keymap *keymap;
        xkb_state *state;

        xkb_mod_index_t shift_mod;
        xkb_mod_index_t caps_mod;
        xkb_mod_index_t ctrl_mod;
        xkb_mod_index_t alt_mod;
        xkb_mod_index_t mod2_mod;
        xkb_mod_index_t mod3_mod;
        xkb_mod_index_t super_mod;
        xkb_mod_index_t mod5_mod;
        xkb_led_index_t num_led;
        xkb_led_index_t caps_led;
        xkb_led_index_t scroll_led;
    } xkb;

    Qt::KeyboardModifiers modifiers;
};

namespace {

const unsigned int connection_id(1);

void registryGlobal(void *data,
                    wl_registry *registry,
                    uint32_t name,
                    const char *interface,
                    uint32_t version)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(registry);
    d->handleRegistryGlobal(name, interface, version);
}

void registryGlobalRemove(void *data,
                          wl_registry *registry,
                          uint32_t name)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(registry);
    d->handleRegistryGlobalRemove(name);
}

const wl_registry_listener maliit_registry_listener = {
    registryGlobal,
    registryGlobalRemove
};

void inputMethodActivate(void *data,
                         input_method *input_method,
                         input_method_context *context,
                         uint32_t serial)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(input_method);
    d->handleInputMethodActivate(context, serial);
}

void inputMethodDeactivate(void *data,
                           input_method *input_method,
                           input_method_context *context)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(input_method);
    d->handleInputMethodDeactivate(context);
}

const input_method_listener maliit_input_method_listener = {
    inputMethodActivate,
    inputMethodDeactivate
};

void inputMethodContextSurroundingText(void *data,
                                       input_method_context *context,
                                       const char *text,
                                       uint32_t cursor,
                                       uint32_t anchor)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextSurroundingText(text, cursor, anchor);
}

void inputMethodContextReset(void *data,
                             input_method_context *context,
                             uint32_t serial)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextReset(serial);
}

void inputMethodContextContentType(void *data,
                                   input_method_context *context,
                                   uint32_t hint,
                                   uint32_t purpose)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextContentType(hint, purpose);
}

void inputMethodContextEnterKeyType(void *data,
                                   input_method_context *context,
                                   uint32_t enter_key_type)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextEnterKeyType(enter_key_type);
}

void inputMethodContextInvokeAction(void *data,
                                    input_method_context *context,
                                    uint32_t button,
                                    uint32_t index)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextInvokeAction(button, index);
}

void inputMethodContextCommit(void *data,
                              input_method_context *context)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextCommit();
}

/* Not implemented in LSM
void inputMethodContextPreferredLanguage(void *data,
                                         input_method_context *context,
                                         const char *language)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextPreferredLanguage(language);
}
*/

void inputMethodContextMaxTextLength(void *data,
                                       input_method_context *context,
                                       uint32_t maxLength)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextMaxTextLength(maxLength);
}

void inputMethodContextPlatformData(void *data,
                                       input_method_context *context,
                                       const char *pattern)
{
    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    Q_UNUSED(context);
    d->handleInputMethodContextPlatformData(pattern);
}
const input_method_context_listener maliit_input_method_context_listener = {
    inputMethodContextSurroundingText,
    inputMethodContextReset,
    inputMethodContextContentType,
    inputMethodContextEnterKeyType,
    inputMethodContextInvokeAction,
    inputMethodContextCommit,
    inputMethodContextMaxTextLength,
    inputMethodContextPlatformData
/* Not implemented in LSM
    inputMethodContextPreferredLanguage
*/
};

text_model_preedit_style face_to_uint (Maliit::PreeditFace face)
{
    switch (face) {
    case Maliit::PreeditDefault:
        return TEXT_MODEL_PREEDIT_STYLE_DEFAULT;

    case Maliit::PreeditNoCandidates:
        return TEXT_MODEL_PREEDIT_STYLE_INCORRECT;

    case Maliit::PreeditKeyPress:
        return TEXT_MODEL_PREEDIT_STYLE_HIGHLIGHT;

    case Maliit::PreeditUnconvertible:
        return TEXT_MODEL_PREEDIT_STYLE_INACTIVE;

    case Maliit::PreeditActive:
        return TEXT_MODEL_PREEDIT_STYLE_ACTIVE;

    default:
/* Not implemented in LSM
        return TEXT_MODEL_PREEDIT_STYLE_NONE;
*/
        return TEXT_MODEL_PREEDIT_STYLE_DEFAULT;
    }
}

Maliit::TextContentType westonPurposeToMaliit(text_model_content_purpose purpose)
{
    switch (purpose) {
    case TEXT_MODEL_CONTENT_PURPOSE_NORMAL:
        return Maliit::FreeTextContentType;

    case TEXT_MODEL_CONTENT_PURPOSE_DIGITS:
    case TEXT_MODEL_CONTENT_PURPOSE_NUMBER:
        return Maliit::NumberContentType;

    case TEXT_MODEL_CONTENT_PURPOSE_PHONE:
        return Maliit::PhoneNumberContentType;

    case TEXT_MODEL_CONTENT_PURPOSE_URL:
        return Maliit::UrlContentType;

    case TEXT_MODEL_CONTENT_PURPOSE_EMAIL:
        return Maliit::EmailContentType;

    default:
        return Maliit::CustomContentType;
    }
}

Maliit::EnterKeyType westonEnterKeyTypeToMaliit(text_model_enter_key_type enter_key_type)
{
    switch (enter_key_type) {
    case TEXT_MODEL_ENTER_KEY_TYPE_DEFAULT:
        return Maliit::DefaultEnterKeyType;

    case TEXT_MODEL_ENTER_KEY_TYPE_RETURN:
        return Maliit::ReturnEnterKeyType;

    case TEXT_MODEL_ENTER_KEY_TYPE_DONE:
        return Maliit::DoneEnterKeyType;

    case TEXT_MODEL_ENTER_KEY_TYPE_GO:
        return Maliit::GoEnterKeyType;

    case TEXT_MODEL_ENTER_KEY_TYPE_SEND:
        return Maliit::SendEnterKeyType;

    case TEXT_MODEL_ENTER_KEY_TYPE_SEARCH:
        return Maliit::SearchEnterKeyType;

    case TEXT_MODEL_ENTER_KEY_TYPE_NEXT:
        return Maliit::NextEnterKeyType;

    case TEXT_MODEL_ENTER_KEY_TYPE_PREVIOUS:
        return Maliit::PreviousEnterKeyType;

    default:
        return Maliit::DefaultEnterKeyType;
    }
}

bool matchesFlag(int value,
                 int flag)
{
    return ((value & flag) == flag);
}

} // unnamed namespace

MInputContextWestonIMProtocolConnectionPrivate::MInputContextWestonIMProtocolConnectionPrivate(MInputContextWestonIMProtocolConnection *connection)
    : q_ptr(connection),
      display(0),
      registry(0),
      im(0),
      im_context(0),
      im_serial(0),
      selection(),
      mods(),
      state_info()
{
    display = static_cast<wl_display *>(QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("display"));
    if (!display) {
        qCritical() << "Failed to get a display.";
        return;
    }
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &maliit_registry_listener, this);
    // QtWayland will do dispatching for us.

    xkb.context = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
    xkb.keymap = 0;
    xkb.state = 0;
}

MInputContextWestonIMProtocolConnectionPrivate::~MInputContextWestonIMProtocolConnectionPrivate()
{
    if (im_context) {
        input_method_context_destroy(im_context);
    }
    if (im) {
        input_method_destroy(im);
    }
    if (registry) {
        wl_registry_destroy(registry);
    }
    if (xkb.state) {
        xkb_state_unref(xkb.state);
    }
    if (xkb.keymap) {
        xkb_keymap_unref(xkb.keymap);
    }
    if (xkb.context) {
        xkb_context_unref(xkb.context);
    }
}

void MInputContextWestonIMProtocolConnectionPrivate::handleRegistryGlobal(uint32_t name,
                                                                          const char *interface,
                                                                          uint32_t version)
{
    Q_UNUSED(version);

    qDebug() << "Name:" << name << "Interface:" << interface;
    if (!strcmp(interface, "input_method")) {
        im = static_cast<input_method *>(wl_registry_bind(registry, name, &input_method_interface, 1));
        input_method_add_listener(im, &maliit_input_method_listener, this);
    }
}

void MInputContextWestonIMProtocolConnectionPrivate::handleRegistryGlobalRemove(uint32_t name)
{
    qDebug() << "Name:" << name;
}

void
inputMethodKeyboardKeyMap(void *data,
                          struct wl_keyboard *wl_keyboard,
                          uint32_t format,
                          int32_t fd,
                          uint32_t size)
{
    Q_UNUSED(wl_keyboard);

    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    d->processKeyMap(format, fd, size);
}

static void
inputMethodKeyboardKey(void *data,
                       struct wl_keyboard *wl_keyboard,
                       uint32_t serial,
                       uint32_t time,
                       uint32_t key,
                       uint32_t state_w)
{
    Q_UNUSED(wl_keyboard);

    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    d->processKeyEvent(serial, time, key, state_w);
}

static void
inputMethodKeyboardModifiers(void *data,
                       struct wl_keyboard *wl_keyboard,
                       uint32_t serial,
                       uint32_t mods_depressed,
                       uint32_t mods_latched,
                       uint32_t mods_locked,
                       uint32_t group)
{
    Q_UNUSED(wl_keyboard);

    MInputContextWestonIMProtocolConnectionPrivate *d =
        static_cast<MInputContextWestonIMProtocolConnectionPrivate *>(data);

    d->processKeyModifiers(serial, mods_depressed, mods_latched, mods_locked, group);
}

const wl_keyboard_listener input_method_keyboard_listener = {
    inputMethodKeyboardKeyMap,
    NULL, /* enter */
    NULL, /* leave */
    inputMethodKeyboardKey,
    inputMethodKeyboardModifiers,
    NULL  /* repeat_info */
};

void MInputContextWestonIMProtocolConnectionPrivate::processKeyMap(uint32_t format, uint32_t fd, uint32_t size)
{
    if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        char *keymapArea = static_cast<char*>(mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));
        if (keymapArea == MAP_FAILED) {
            close(fd);
            qWarning() << "failed to mmap() " << (unsigned long) size << " bytes\n";
            return;
        }

        xkb_keymap *newKeymap = xkb_keymap_new_from_string(xkb.context,
                keymapArea, XKB_KEYMAP_FORMAT_TEXT_V1,
                XKB_MAP_COMPILE_PLACEHOLDER);

        munmap(keymapArea, size);
        close(fd);

        // free existing keymap
        if (xkb.keymap) {
            xkb_keymap_unref(xkb.keymap);
        }
        xkb.keymap = newKeymap;

        if (xkb.state) {
            xkb_state_unref(xkb.state);
        }
        xkb.state = xkb_state_new(xkb.keymap);

        // set modifier index
        xkb.shift_mod = xkb_map_mod_get_index(xkb.keymap, XKB_MOD_NAME_SHIFT);
        xkb.caps_mod = xkb_map_mod_get_index(xkb.keymap, XKB_MOD_NAME_CAPS);
        xkb.ctrl_mod = xkb_map_mod_get_index(xkb.keymap, XKB_MOD_NAME_CTRL);
        xkb.alt_mod = xkb_map_mod_get_index(xkb.keymap, XKB_MOD_NAME_ALT);
        xkb.mod2_mod = xkb_map_mod_get_index(xkb.keymap, "Mod2");
        xkb.mod3_mod = xkb_map_mod_get_index(xkb.keymap, "Mod3");
        xkb.super_mod = xkb_map_mod_get_index(xkb.keymap, XKB_MOD_NAME_LOGO);
        xkb.mod5_mod = xkb_map_mod_get_index(xkb.keymap, "Mod5");

        xkb.num_led = xkb_map_led_get_index(xkb.keymap, XKB_LED_NAME_NUM);
        xkb.caps_led = xkb_map_led_get_index(xkb.keymap, XKB_LED_NAME_CAPS);
        xkb.scroll_led = xkb_map_led_get_index(xkb.keymap, XKB_LED_NAME_SCROLL);
    }
}

struct RemoteKeysym {
    uint32_t key;
    xkb_keysym_t keysym;
};

static const struct RemoteKeysym g_RemoteKeysymMap[] = {
    { KEY_PREVIOUS, XKB_KEY_XF86Back }, // back button in RC

    { KEY_STOP, XKB_KEY_XF86AudioStop },
    { KEY_PLAY, XKB_KEY_XF86AudioPlay },
    { KEY_REWIND, XKB_KEY_XF86AudioRewind },
    { KEY_FASTFORWARD, XKB_KEY_XF86AudioForward },
    { KEY_RECORD, XKB_KEY_XF86AudioRecord },

    // IR numeric buttons
    { KEY_NUMERIC_0, XKB_KEY_0 },
    { KEY_NUMERIC_1, XKB_KEY_1 },
    { KEY_NUMERIC_2, XKB_KEY_2 },
    { KEY_NUMERIC_3, XKB_KEY_3 },
    { KEY_NUMERIC_4, XKB_KEY_4 },
    { KEY_NUMERIC_5, XKB_KEY_5 },
    { KEY_NUMERIC_6, XKB_KEY_6 },
    { KEY_NUMERIC_7, XKB_KEY_7 },
    { KEY_NUMERIC_8, XKB_KEY_8 },
    { KEY_NUMERIC_9, XKB_KEY_9 },
};

static xkb_keysym_t get_remote_keysym(uint32_t key)
{
    for (unsigned i = 0; i < sizeof(g_RemoteKeysymMap) / sizeof(RemoteKeysym); i++) {
        if (key == g_RemoteKeysymMap[i].key)
            return g_RemoteKeysymMap[i].keysym;
    }
    return XKB_KEY_NoSymbol;
}

#ifdef HAS_LIBIM
struct LGRemoteKey {
    uint32_t lgKey;
    uint32_t normalKey;
};

static const struct LGRemoteKey g_LGRemoteKeyMap[] = {
    { IR_KEY_BS_NUM_1, KEY_NUMERIC_1 },
    { IR_KEY_BS_NUM_2, KEY_NUMERIC_2 },
    { IR_KEY_BS_NUM_3, KEY_NUMERIC_3 },
    { IR_KEY_BS_NUM_4, KEY_NUMERIC_4 },
    { IR_KEY_BS_NUM_5, KEY_NUMERIC_5 },
    { IR_KEY_BS_NUM_6, KEY_NUMERIC_6 },
    { IR_KEY_BS_NUM_7, KEY_NUMERIC_7 },
    { IR_KEY_BS_NUM_8, KEY_NUMERIC_8 },
    { IR_KEY_BS_NUM_9, KEY_NUMERIC_9 },
    { IR_KEY_BS_NUM_10, KEY_NUMERIC_0 },

    { IR_KEY_CS1_NUM_1, KEY_NUMERIC_1 },
    { IR_KEY_CS1_NUM_2, KEY_NUMERIC_2 },
    { IR_KEY_CS1_NUM_3, KEY_NUMERIC_3 },
    { IR_KEY_CS1_NUM_4, KEY_NUMERIC_4 },
    { IR_KEY_CS1_NUM_5, KEY_NUMERIC_5 },
    { IR_KEY_CS1_NUM_6, KEY_NUMERIC_6 },
    { IR_KEY_CS1_NUM_7, KEY_NUMERIC_7 },
    { IR_KEY_CS1_NUM_8, KEY_NUMERIC_8 },
    { IR_KEY_CS1_NUM_9, KEY_NUMERIC_9 },
    { IR_KEY_CS1_NUM_10, KEY_NUMERIC_0 },

    { IR_KEY_CS2_NUM_1, KEY_NUMERIC_1 },
    { IR_KEY_CS2_NUM_2, KEY_NUMERIC_2 },
    { IR_KEY_CS2_NUM_3, KEY_NUMERIC_3 },
    { IR_KEY_CS2_NUM_4, KEY_NUMERIC_4 },
    { IR_KEY_CS2_NUM_5, KEY_NUMERIC_5 },
    { IR_KEY_CS2_NUM_6, KEY_NUMERIC_6 },
    { IR_KEY_CS2_NUM_7, KEY_NUMERIC_7 },
    { IR_KEY_CS2_NUM_8, KEY_NUMERIC_8 },
    { IR_KEY_CS2_NUM_9, KEY_NUMERIC_9 },
    { IR_KEY_CS2_NUM_10, KEY_NUMERIC_0 },

    { IR_KEY_TER_NUM_1, KEY_NUMERIC_1 },
    { IR_KEY_TER_NUM_2, KEY_NUMERIC_2 },
    { IR_KEY_TER_NUM_3, KEY_NUMERIC_3 },
    { IR_KEY_TER_NUM_4, KEY_NUMERIC_4 },
    { IR_KEY_TER_NUM_5, KEY_NUMERIC_5 },
    { IR_KEY_TER_NUM_6, KEY_NUMERIC_6 },
    { IR_KEY_TER_NUM_7, KEY_NUMERIC_7 },
    { IR_KEY_TER_NUM_8, KEY_NUMERIC_8 },
    { IR_KEY_TER_NUM_9, KEY_NUMERIC_9 },
    { IR_KEY_TER_NUM_10, KEY_NUMERIC_0 },

    { IR_KEY_4K_BS_NUM_1, KEY_NUMERIC_1 },
    { IR_KEY_4K_BS_NUM_2, KEY_NUMERIC_2 },
    { IR_KEY_4K_BS_NUM_3, KEY_NUMERIC_3 },
    { IR_KEY_4K_BS_NUM_4, KEY_NUMERIC_4 },
    { IR_KEY_4K_BS_NUM_5, KEY_NUMERIC_5 },
    { IR_KEY_4K_BS_NUM_6, KEY_NUMERIC_6 },
    { IR_KEY_4K_BS_NUM_7, KEY_NUMERIC_7 },
    { IR_KEY_4K_BS_NUM_8, KEY_NUMERIC_8 },
    { IR_KEY_4K_BS_NUM_9, KEY_NUMERIC_9 },
    { IR_KEY_4K_BS_NUM_10, KEY_NUMERIC_0 },

    { IR_KEY_4K_CS_NUM_1, KEY_NUMERIC_1 },
    { IR_KEY_4K_CS_NUM_2, KEY_NUMERIC_2 },
    { IR_KEY_4K_CS_NUM_3, KEY_NUMERIC_3 },
    { IR_KEY_4K_CS_NUM_4, KEY_NUMERIC_4 },
    { IR_KEY_4K_CS_NUM_5, KEY_NUMERIC_5 },
    { IR_KEY_4K_CS_NUM_6, KEY_NUMERIC_6 },
    { IR_KEY_4K_CS_NUM_7, KEY_NUMERIC_7 },
    { IR_KEY_4K_CS_NUM_8, KEY_NUMERIC_8 },
    { IR_KEY_4K_CS_NUM_9, KEY_NUMERIC_9 },
    { IR_KEY_4K_CS_NUM_10, KEY_NUMERIC_0 },

};

static uint32_t check_lgremote_key(uint32_t key)
{
    // Immediately drop normal keys
    if (key < KEY_LG_BASE)
        return key;

    for (unsigned i = 0; i < sizeof(g_LGRemoteKeyMap) / sizeof(LGRemoteKey); i++) {
        if (key == g_LGRemoteKeyMap[i].lgKey)
            return g_LGRemoteKeyMap[i].normalKey;
    }

    return key;
}

static bool is_lgremote_numbersign(uint32_t key)
{
    return (key == IR_KEY_BS_NUM_11 ||
            key == IR_KEY_CS1_NUM_11 ||
            key == IR_KEY_CS2_NUM_11 ||
            key == IR_KEY_TER_NUM_11);
}

static bool is_lgremote_asterisk(uint32_t key)
{
    return (key == IR_KEY_BS_NUM_12 ||
            key == IR_KEY_CS1_NUM_12 ||
            key == IR_KEY_CS2_NUM_12 ||
            key == IR_KEY_TER_NUM_12);
}
#endif

void MInputContextWestonIMProtocolConnectionPrivate::processKeyEvent(uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    Q_Q(MInputContextWestonIMProtocolConnection);
    Q_UNUSED(serial);

    if (key == 4294967288) {
        // This key is sent from voice service even when STT is not active.
        // As this is received as hardware key event, plugins may misbehave
        // by recognizing this event as hardware input and go to HID mode.
        // So for this case escape here and do not send this event to plugins.
        return;
    }

    const int EVDEV_OFFSET = 8;

    QEvent::Type keyType = (state == WL_KEYBOARD_KEY_STATE_RELEASED ? QEvent::KeyRelease : QEvent::KeyPress);
#ifdef HAS_LIBIM
    key = check_lgremote_key(key);
#endif

    const xkb_keysym_t *syms;
    int num_syms = xkb_key_get_syms(xkb.state, key + EVDEV_OFFSET, &syms);

    xkb_keysym_t sym = XKB_KEY_NoSymbol;
    if (1 == num_syms)
        sym = syms[0];
    // TODO: multiple key press?

    // Check keysym mapping for RC buttons
    if (sym == XKB_KEY_NoSymbol)
        sym = get_remote_keysym(key);

    int keyCode = xkbKeyToQtKey(sym);

    if (isKeypadKey(sym))
        modifiers |= Qt::KeypadModifier;

    QString text("");
    if ((XKB_KEY_A <= sym && sym <= XKB_KEY_Z) ||
        (XKB_KEY_a <= sym && sym <= XKB_KEY_z)) {
        text.append(QChar(sym));
    }

#ifdef HAS_LIBIM
    if (is_lgremote_numbersign(key)) {
        // # = shift + 3
        q->processKeyEvent(connection_id, keyType, Qt::Key_NumberSign,
                modifiers | Qt::ShiftModifier, text,
                false, 0, KEY_3 + EVDEV_OFFSET, 0, time);
    } else if (is_lgremote_asterisk(key)) {
        // * = shift + 8
        q->processKeyEvent(connection_id, keyType, Qt::Key_Asterisk,
                modifiers | Qt::ShiftModifier, text,
                false, 0, KEY_8 + EVDEV_OFFSET, 0, time);
    } else
#endif
    {
        q->processKeyEvent(connection_id, keyType, static_cast<Qt::Key>(keyCode),
                modifiers, text,
                false, 0, key + EVDEV_OFFSET, 0, time);
    }
}

void MInputContextWestonIMProtocolConnectionPrivate::processKeyModifiers(uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    Q_UNUSED(serial);

    uint32_t mods_lookup = mods_depressed | mods_latched;
    modifiers = Qt::NoModifier;
    if (mods_lookup & (1 << xkb.ctrl_mod))
        modifiers |= Qt::ControlModifier;
    if (mods_lookup & (1 << xkb.alt_mod))
        modifiers |= Qt::AltModifier;
    if (mods_lookup & (1 << xkb.shift_mod))
        modifiers |= Qt::ShiftModifier;

    xkb_state_update_mask(xkb.state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodActivate(input_method_context *context,
                                                                               uint32_t serial)
{
    Q_Q(MInputContextWestonIMProtocolConnection);

    qDebug() << "context:" << (long) context << "serial:" << serial;
    if (im_context) {
        input_method_context_destroy(im_context);
    }
    im_context = context;
    im_serial = serial;
    input_method_context_add_listener(im_context, &maliit_input_method_context_listener, this);

    wl_keyboard *keyboard = input_method_context_grab_keyboard(im_context);
    wl_keyboard_add_listener(keyboard, &input_method_keyboard_listener, this);

    input_method_context_modifiers_map(im_context, mods.getModMap());

    state_info[FocusStateAttribute] = true;

    //Even if user hasn't set these property, followings should have a default value.
    //See GlobalInputMethod::show() in imemanager.
    state_info[ContentTypeAttribute] = Maliit::FreeTextContentType;
    state_info[EnterKeyTypeAttribute] = Maliit::DefaultEnterKeyType;

    q->updateWidgetInformation(connection_id, state_info, true);
    q->activateContext(connection_id);
    q->showInputMethod(connection_id);
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodDeactivate(input_method_context *context)
{
    Q_Q(MInputContextWestonIMProtocolConnection);
    qDebug() << "context" << (long)context;
    if (!im_context) {
        return;
    }
    input_method_context_destroy(im_context);
    im_context = NULL;
    state_info.clear();
    state_info[FocusStateAttribute] = false;
    q->updateWidgetInformation(connection_id, state_info, true);
    q->hideInputMethod(connection_id);
    q->handleDisconnection(connection_id);
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextSurroundingText(const char *text,
                                                                                             uint32_t cursor,
                                                                                             uint32_t anchor)
{
    Q_Q(MInputContextWestonIMProtocolConnection);

    qDebug() << "text:" << text << "cursor:" << cursor << "anchor:" << anchor;

    int len = strlen(text);
    cursor = len < (int) cursor ? len : cursor;
    anchor = len < (int) anchor ? len : anchor;

    state_info[SurroundingTextAttribute] = QString(text);
    state_info[CursorPositionAttribute] = cursor;
    state_info[AnchorPositionAttribute] = anchor;
    state_info[HasSelectionAttribute] = (cursor != anchor);
    if (cursor == anchor) {
        selection.clear();
    } else {
        uint32_t begin(qMin(cursor, anchor));
        uint32_t end(qMax(cursor, anchor));

        selection = QString::fromUtf8(text + begin, end - begin);
    }
    q->updateWidgetInformation(connection_id, state_info, false);
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextReset(uint32_t serial)
{
    Q_Q(MInputContextWestonIMProtocolConnection);

    qDebug() << "serial:" << serial;
    im_serial = serial;
    q->reset(connection_id);
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextContentType(uint32_t hint,
                                                                                         uint32_t purpose)
{
    Q_Q(MInputContextWestonIMProtocolConnection);

    qDebug() << "hint:" << hint << "purpose:" << purpose;

    state_info[ContentTypeAttribute] = westonPurposeToMaliit(static_cast<text_model_content_purpose>(purpose));
    state_info[AutoCapitalizationAttribute] = matchesFlag(hint, TEXT_MODEL_CONTENT_HINT_AUTO_CAPITALIZATION);
    state_info[CorrectionAttribute] = matchesFlag(hint, TEXT_MODEL_CONTENT_HINT_AUTO_CORRECTION);
    state_info[PredictionAttribute] = matchesFlag(hint, TEXT_MODEL_CONTENT_HINT_AUTO_COMPLETION);
    state_info[HiddenTextAttribute] = matchesFlag(hint, TEXT_MODEL_CONTENT_HINT_HIDDEN_TEXT)
        || matchesFlag(hint, TEXT_MODEL_CONTENT_HINT_PASSWORD)
        || matchesFlag(hint, TEXT_MODEL_CONTENT_HINT_SENSITIVE_DATA)
        || (purpose == TEXT_MODEL_CONTENT_PURPOSE_PASSWORD);

    q->updateWidgetInformation(connection_id, state_info, false);
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextEnterKeyType(uint32_t enter_key_type)
{
    Q_Q(MInputContextWestonIMProtocolConnection);

    qDebug() << "enter_key_type:" << enter_key_type;

    state_info[EnterKeyTypeAttribute] = westonEnterKeyTypeToMaliit(static_cast<text_model_enter_key_type>(enter_key_type));

    q->updateWidgetInformation(connection_id, state_info, false);
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextInvokeAction(uint32_t button,
                                                                                          uint32_t index)
{
    qDebug() << "button:" << button << "index:" << index;
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextCommit()
{
    qDebug() << "commit";
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextPreferredLanguage(const char *language)
{
    qDebug() << "language: " << language;
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextMaxTextLength(uint32_t maxLength)
{
    Q_Q(MInputContextWestonIMProtocolConnection);

    qDebug() << "maxLength:" << maxLength;
    state_info[MaxTextLengthAttribute] = maxLength;
    q->updateWidgetInformation(connection_id, state_info, false);
}

void MInputContextWestonIMProtocolConnectionPrivate::handleInputMethodContextPlatformData(const char *pattern)
{
    Q_Q(MInputContextWestonIMProtocolConnection);

    qDebug() << "pattern:" << pattern;
    state_info[PlatformDataAttribute] = pattern;
    q->updateWidgetInformation(connection_id, state_info, false);
}

// MInputContextWestonIMProtocolConnection

MInputContextWestonIMProtocolConnection::MInputContextWestonIMProtocolConnection()
    : d_ptr(new MInputContextWestonIMProtocolConnectionPrivate(this))
{
}

MInputContextWestonIMProtocolConnection::~MInputContextWestonIMProtocolConnection()
{
}

void MInputContextWestonIMProtocolConnection::sendPreeditString(const QString &string,
                                                                const QList<Maliit::PreeditTextFormat> &preedit_formats,
                                                                int replace_start,
                                                                int replace_length,
                                                                int cursor_pos)
{
    Q_D(MInputContextWestonIMProtocolConnection);

    qDebug() << "Preedit:" << string
             << "replace start:" << replace_start
             << "replace length:" << replace_length
             << "cursor position:" << cursor_pos;

    if (d->im_context) {
        MInputContextConnection::sendPreeditString(string, preedit_formats,
                                                   replace_start, replace_length,
                                                   cursor_pos);
        const QByteArray raw(string.toUtf8());

        if (replace_length > 0) {
            input_method_context_delete_surrounding_text(d->im_context, d->im_serial,
                                                         replace_start, replace_length);
        }
        Q_FOREACH (const Maliit::PreeditTextFormat& format, preedit_formats) {
            input_method_context_preedit_styling(d->im_context, d->im_serial,
                                                 format.start, format.length,
                                                 face_to_uint (format.preeditFace));
        }
        if (cursor_pos < 0) {
            cursor_pos = string.size() + 1 - cursor_pos;
        }
        input_method_context_preedit_cursor(d->im_context, d->im_serial,
                                            // convert from internal pos to byte pos
                                            string.left(cursor_pos).toUtf8().size());
        input_method_context_preedit_string(d->im_context, d->im_serial, raw.data(),
                                            raw.data());
    }
}

int MInputContextWestonIMProtocolConnection::contentType(bool &valid)
{
    qDebug() << "valid:" << valid;
    int type = MInputContextConnection::contentType(valid);
    return type;
}

int MInputContextWestonIMProtocolConnection::enterKeyType(bool &valid)
{
    qDebug() << "valid:" << valid;
    int type = MInputContextConnection::enterKeyType(valid);
    return type;
}

bool MInputContextWestonIMProtocolConnection::surroundingText(QString &text, int &cursor_position)
{
    qDebug() << "text:" << text << "cursor_position:" << cursor_position;
    bool result = MInputContextConnection::surroundingText(text, cursor_position);
    return result;
}

bool MInputContextWestonIMProtocolConnection::hasSelection(bool &valid)
{
    qDebug() << "valid:" << valid;
    bool result = MInputContextConnection::hasSelection(valid);
    return result;
}

bool MInputContextWestonIMProtocolConnection::focusState(bool &valid)
{
    qDebug() << "valid:" << valid;
    bool result = MInputContextConnection::focusState(valid);
    return result;
}

bool MInputContextWestonIMProtocolConnection::correctionEnabled(bool &valid)
{
    qDebug() << "valid:" << valid;
    bool result = MInputContextConnection::correctionEnabled(valid);
    return result;
}

bool MInputContextWestonIMProtocolConnection::predictionEnabled(bool &valid)
{
    qDebug() << "valid:" << valid;
    bool result = MInputContextConnection::predictionEnabled(valid);
    return result;
}

bool MInputContextWestonIMProtocolConnection::autoCapitalizationEnabled(bool &valid)
{
    qDebug() << "valid:" << valid;
    bool result = MInputContextConnection::autoCapitalizationEnabled(valid);
    return result;
}

bool MInputContextWestonIMProtocolConnection::hiddenText(bool &valid)
{
    qDebug() << "valid:" << valid;
    bool result = MInputContextConnection::hiddenText(valid);
    return result;
}

int MInputContextWestonIMProtocolConnection::anchorPosition(bool &valid)
{
    qDebug() << "valid:" << valid;
    bool result = MInputContextConnection::anchorPosition(valid);
    return result;
}

void MInputContextWestonIMProtocolConnection::sendCommitString(const QString &string,
                                                               int replace_start,
                                                               int replace_length,
                                                               int cursor_pos)
{
    Q_D(MInputContextWestonIMProtocolConnection);

    qDebug() << "commit:" << string
             << "replace start:" << replace_start
             << "replace length:" << replace_length
             << "cursor position:" << cursor_pos;

    if (d->im_context) {
        MInputContextConnection::sendCommitString(string, replace_start, replace_length, cursor_pos);
        const QByteArray raw(string.toUtf8());

        if (cursor_pos < 0) {
            cursor_pos = string.size();
        }
        input_method_context_preedit_string(d->im_context, d->im_serial, "", "");
        // NOTE: length is unsigned in wayland protocol
        if (replace_length != 0) {
            input_method_context_delete_surrounding_text(d->im_context, d->im_serial,
                                                         replace_start, replace_length);
        }
        const int pos = 0; // TODO (string.left(cursor_pos).toUtf8().size());

        input_method_context_cursor_position (d->im_context, d->im_serial, pos, pos);
        input_method_context_commit_string(d->im_context, d->im_serial, raw.data());
                                           //string.left(cursor_pos).toUtf8().size());
    }
}


void MInputContextWestonIMProtocolConnection::sendKeyEvent(const QKeyEvent &keyEvent,
                                                           Maliit::EventRequestType requestType)
{
    Q_D(MInputContextWestonIMProtocolConnection);

    qDebug() << "key:" << keyEvent.key()
             << "requestType:" << requestType;

    if (d->im_context) {
        xkb_keysym_t key_sym(qtKeyToXkbKey(keyEvent.key()));

        if (!key_sym) {
            qWarning() << "No conversion from Qt::Key:" << keyEvent.key() << "to XKB key. Update the qtKeyToXkbKey function.";
            return;
        }

        wl_keyboard_key_state state;

        switch (keyEvent.type()) {
        case QEvent::KeyPress:
            state = WL_KEYBOARD_KEY_STATE_PRESSED;
            break;

        case QEvent::KeyRelease:
            state = WL_KEYBOARD_KEY_STATE_RELEASED;
            break;

        default:
            qWarning() << "Unknown QKeyEvent type:" << keyEvent.type();
            return;
        }

        xkb_mod_mask_t mod_mask(d->mods.fromQt(keyEvent.modifiers()));

        MInputContextConnection::sendKeyEvent(keyEvent, requestType);

        qDebug() << "key_sym:" << key_sym << "state:" << state << "mod mask:" << mod_mask;

        input_method_context_keysym(d->im_context, d->im_serial, keyEvent.timestamp(),
                                    key_sym, state, mod_mask);
    }
}

/* Rather not yet implemented in Weston.
void MInputContextWestonIMProtocolConnection::notifyImInitiatedHiding()
{
    Q_D(MInputContextWestonIMProtocol);

    if (d->im_context) {
        input_method_context_destroy(d->im_context);
        d->im_context = 0;
    }
}

void MInputContextWestonIMProtocolConnection::setGlobalCorrectionEnabled(bool enabled)
{
    if ((enabled != globalCorrectionEnabled()) && activeContext()) {
        dbus_g_proxy_call_no_reply(activeContext()->inputContextProxy, "setGlobalCorrectionEnabled",
                                   G_TYPE_BOOLEAN, enabled,
                                   G_TYPE_INVALID);

        MInputContextConnection::setGlobalCorrectionEnabled(enabled);
    }
}
*/

QString MInputContextWestonIMProtocolConnection::selection(bool &valid)
{
    Q_D(MInputContextWestonIMProtocolConnection);

    valid = !d->selection.isEmpty();
    return d->selection;
}
/*
void MInputContextWestonIMProtocolConnection::setLanguage(const QString &language)
{
    Q_D(MInputContextWestonIMProtocolConnection);

    if (d->im_context) {
        input_method_context_language(d->im_context, d->im_serial,
                                      language.toUtf8().data());
    }
}
*/
/* Not implemented in Weston
QRect MInputContextWestonIMProtocolConnection::preeditRectangle(bool &valid)
{
    GError *error = NULL;

    gboolean gvalidity;
    gint32 x, y, width, height;

    if (activeContext() &&
        dbus_g_proxy_call(activeContext()->inputContextProxy, "preeditRectangle", &error, G_TYPE_INVALID,
                          G_TYPE_BOOLEAN, &gvalidity, G_TYPE_INT, &x, G_TYPE_INT, &y,
                          G_TYPE_INT, &width, G_TYPE_INT, &height, G_TYPE_INVALID)) {
        valid = gvalidity == TRUE;
    } else {
        if (error) { // dbus_g_proxy_call may return FALSE and not set error despite what the doc says
            g_error_free(error);
        }
        valid = false;
        return QRect();
    }

    return QRect(x, y, width, height);
}

void MInputContextWestonIMProtocolConnection::setRedirectKeys(bool enabled)
{
    if ((redirectKeysEnabled() != enabled) && activeContext()) {
        dbus_g_proxy_call_no_reply(activeContext()->inputContextProxy, "setRedirectKeys",
                                   G_TYPE_BOOLEAN, enabled ? TRUE : FALSE,
                                   G_TYPE_INVALID);

        MInputContextConnection::setRedirectKeys(enabled);
    }
}

void MInputContextWestonIMProtocolConnection::setDetectableAutoRepeat(bool enabled)
{
    if ((detectableAutoRepeat() != enabled) && activeContext()) {
        dbus_g_proxy_call_no_reply(activeContext()->inputContextProxy, "setDetectableAutoRepeat",
                                   G_TYPE_BOOLEAN, enabled,
                                   G_TYPE_INVALID);

        MInputContextConnection::setDetectableAutoRepeat(enabled);
    }
}

void MInputContextWestonIMProtocolConnection::invokeAction(const QString &action, const QKeySequence &sequence)
{
    if (activeContext()) {
        DBusMessage *message = dbus_message_new_signal(DBusPath,
                                                       "com.meego.inputmethod.uiserver1",
                                                       "invokeAction");
        char *action_string = strdup(action.toUtf8().constData());
        char *sequence_string = strdup(sequence.toString(QKeySequence::PortableText).toUtf8().constData());
        dbus_message_append_args(message,
                                 DBUS_TYPE_STRING, &action_string,
                                 DBUS_TYPE_STRING, &sequence_string,
                                 DBUS_TYPE_INVALID);
        free(action_string);
        free(sequence_string);
        dbus_connection_send(dbus_g_connection_get_connection(activeContext()->dbusConnection),
                             message,
                             NULL);
        dbus_message_unref(message);
    }
}
*/

void MInputContextWestonIMProtocolConnection::setSelection(int start, int length)
{
    Q_D (MInputContextWestonIMProtocolConnection);

    if (d->im_context) {
        QString text(d->state_info[SurroundingTextAttribute].toString());
        int byte_index(text.left(start).toUtf8().size());
        int byte_length(text.mid(start, length).toUtf8().size());

        input_method_context_cursor_position (d->im_context, d->im_serial,
                                              byte_index, byte_length);
    }
}

/* Not implemented in Weston
void MInputContextWestonIMProtocolConnection::updateInputMethodArea(const QRegion &region)
{
    if (activeContext()) {
        QRect rect = region.boundingRect();
        dbus_g_proxy_call_no_reply(activeContext()->inputContextProxy, "updateInputMethodArea",
                                   G_TYPE_INT, rect.left(),
                                   G_TYPE_INT, rect.top(),
                                   G_TYPE_INT, rect.width(),
                                   G_TYPE_INT, rect.height(),
                                   G_TYPE_INVALID);
    }
}

void MInputContextWestonIMProtocolConnection::notifyExtendedAttributeChanged(int id,
                                                                     const QString &target,
                                                                     const QString &targetItem,
                                                                     const QString &attribute,
                                                                     const QVariant &value)
{
    if (!activeContext()) {
        return;
    }
    GValue valueData = {0, {{0}, {0}}};
    if (!encodeVariant(&valueData, value)) {
        return;
    }

    dbus_g_proxy_call_no_reply(activeContext()->inputContextProxy, "notifyExtendedAttributeChanged",
                               G_TYPE_INT, id,
                               G_TYPE_STRING, target.toUtf8().data(),
                               G_TYPE_STRING, targetItem.toUtf8().data(),
                               G_TYPE_STRING, attribute.toUtf8().data(),
                               G_TYPE_VALUE, &valueData,
                               G_TYPE_INVALID);
    g_value_unset(&valueData);
}

void MInputContextWestonIMProtocolConnection::notifyExtendedAttributeChanged(const QList<int> &clientIds,
                                                                     int id,
                                                                     const QString &target,
                                                                     const QString &targetItem,
                                                                     const QString &attribute,
                                                                     const QVariant &value)
{
    GValue valueData = {0, {{0}, {0}}};
    if (!encodeVariant(&valueData, value)) {
        return;
    }
    Q_FOREACH (int clientId, clientIds) {
        dbus_g_proxy_call_no_reply(connectionObj(clientId)->inputContextProxy, "notifyExtendedAttributeChanged",
                                   G_TYPE_INT, id,
                                   G_TYPE_STRING, target.toUtf8().data(),
                                   G_TYPE_STRING, targetItem.toUtf8().data(),
                                   G_TYPE_STRING, attribute.toUtf8().data(),
                                   G_TYPE_VALUE, &valueData,
                                   G_TYPE_INVALID);
    }
    g_value_unset(&valueData);
}


void MInputContextWestonIMProtocolConnection::pluginSettingsLoaded(int clientId, const QList<MImPluginSettingsInfo> &info)
{
    MDBusGlibICConnection *client = connectionObj(clientId);
    if (!client) {
        return;
    }

    GType settingsType;
    GPtrArray *settingsData;
    if (!encodeSettings(&settingsType, &settingsData, info)) {
        return;
    }

    dbus_g_proxy_call_no_reply(client->inputContextProxy, "pluginSettingsLoaded",
                               settingsType, settingsData,
                               G_TYPE_INVALID);
    dbus_g_type_collection_peek_vtable(settingsType)->base_vtable.free_func(settingsType, settingsData);
}


void MInputContextWestonIMProtocolConnection::sendActivationLostEvent()
{
    if (activeContext()) {
        dbus_g_proxy_call_no_reply(activeContext()->inputContextProxy, "activationLostEvent",
                                   G_TYPE_INVALID);
    }
}
*/
