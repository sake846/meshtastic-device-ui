#include "graphics/DeviceGUI.h"
#include "graphics/driver/DisplayDriver.h"
#include "graphics/driver/DisplayDriverConfig.h"
#include "input/I2CKeyboardScanner.h"
#include "input/InputDriver.h"

#include "input/I2CKeyboardInputDriver.h"
static I2CKeyboardInputDriver *keyboardDriver = nullptr;

#if LV_USE_LIBINPUT
#include "input/LinuxInputDriver.h"
static LinuxInputDriver *linuxInputDriver = nullptr;
#else
#if defined(INPUTDRIVER_ENCODER_TYPE)
#include "input/EncoderInputDriver.h"
static EncoderInputDriver *encoderDriver = nullptr;
#endif
#if defined(INPUTDRIVER_MATRIX_TYPE)
#include "input/KeyMatrixInputDriver.h"
static KeyMatrixInputDriver *keyMatrixDriver = nullptr;
#endif
#if defined(INPUTDRIVER_BUTTON_TYPE)
#include "input/ButtonInputDriver.h"
static ButtonInputDriver *buttonDriver = nullptr;
#endif
#if defined(INPUTDRIVER_DIRECTIONALPAD_TYPE)
#include "input/DirectionalPadInputDriver.h"
static DirectionalPadInputDriver *padDriver = nullptr;
#endif
#endif
#include "lvgl.h"
#include "ui.h"
#if defined(INPUTDRIVER_ROTARY_TYPE)
#include <Arduino.h>
#include "RotaryEncoder.h"

extern bool pagerFunctionModifierActive;
extern void pagerClearFunctionModifier();
static lv_obj_t *pagerActiveMenuButton = nullptr;

void pagerSetActiveMenuButton(lv_obj_t *button)
{
    pagerActiveMenuButton = button;
}

static int pagerMenuButtonIndex(lv_obj_t *obj)
{
    if (!obj)
        return -1;
    if (obj == objects.home_button) return 0;
    if (obj == objects.nodes_button) return 1;
    if (obj == objects.groups_button) return 2;
    if (obj == objects.messages_button) return 3;
    if (obj == objects.map_button) return 4;
    if (obj == objects.settings_button) return 5;
    return -1;
}

static lv_obj_t *pagerNodeButtonInRow(lv_group_t *group, lv_obj_t *row)
{
    if (!row || lv_obj_get_parent(row) != objects.nodes_panel)
        return nullptr;
    for (lv_obj_t *parent = row; parent; parent = lv_obj_get_parent(parent)) {
        if (lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN))
            return nullptr;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); ++i) {
        lv_obj_t *child = lv_obj_get_child(row, i);
        if (lv_obj_check_type(child, &lv_button_class) && lv_obj_get_group(child) == group &&
            !(lv_obj_get_state(child) & LV_STATE_DISABLED))
            return child;
    }
    return nullptr;
}

static bool pagerMoveNodeFocus(lv_group_t *group, int16_t diff)
{
    lv_obj_t *focused = group ? lv_group_get_focused(group) : nullptr;
    lv_obj_t *focusedRow = focused ? lv_obj_get_parent(focused) : nullptr;
    if (pagerNodeButtonInRow(group, focusedRow) != focused)
        return false;

    int count = 0;
    int current = -1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(objects.nodes_panel); ++i) {
        lv_obj_t *button = pagerNodeButtonInRow(group, lv_obj_get_child(objects.nodes_panel, i));
        if (!button)
            continue;
        if (button == focused)
            current = count;
        ++count;
    }
    if (current < 0 || count == 0)
        return false;

    int targetIndex = (current + diff % count + count) % count;
    for (uint32_t i = 0; i < lv_obj_get_child_count(objects.nodes_panel); ++i) {
        lv_obj_t *button = pagerNodeButtonInRow(group, lv_obj_get_child(objects.nodes_panel, i));
        if (!button)
            continue;
        if (targetIndex-- == 0) {
            lv_group_focus_obj(button);
            break;
        }
    }
    return true;
}

static lv_obj_t *pagerMapFocusButtonAt(int index)
{
    switch (index) {
    case 0: return objects.gps_lock_button;
    case 1: return objects.zoom_in_button;
    case 2: return objects.zoom_out_button;
    case 3: return objects.arrow_up_button;
    case 4: return objects.arrow_left_button;
    case 5: return objects.nav_button;
    case 6: return objects.arrow_right_button;
    case 7: return objects.arrow_down_button;
    default: return nullptr;
    }
}

static int pagerMapFocusButtonIndex(lv_obj_t *obj)
{
    for (int i = 0; i < 8; ++i) {
        if (pagerMapFocusButtonAt(i) == obj)
            return i;
    }
    return -1;
}

static bool pagerMapFocusButtonAvailable(lv_group_t *group, lv_obj_t *button)
{
    if (!button || lv_obj_get_group(button) != group || (lv_obj_get_state(button) & LV_STATE_DISABLED))
        return false;
    for (lv_obj_t *parent = button; parent; parent = lv_obj_get_parent(parent)) {
        if (lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN))
            return false;
    }
    return true;
}

static bool pagerMoveMapFocus(lv_group_t *group, int16_t diff)
{
    lv_obj_t *focused = group ? lv_group_get_focused(group) : nullptr;
    if (pagerMapFocusButtonIndex(focused) < 0)
        return false;

    int count = 0;
    int current = -1;
    for (int i = 0; i < 8; ++i) {
        lv_obj_t *button = pagerMapFocusButtonAt(i);
        if (!pagerMapFocusButtonAvailable(group, button))
            continue;
        if (button == focused)
            current = count;
        ++count;
    }
    if (current < 0 || count == 0)
        return false;

    int targetIndex = (current + diff % count + count) % count;
    for (int i = 0; i < 8; ++i) {
        lv_obj_t *button = pagerMapFocusButtonAt(i);
        if (!pagerMapFocusButtonAvailable(group, button))
            continue;
        if (targetIndex-- == 0) {
            lv_group_focus_obj(button);
            break;
        }
    }
    return true;
}

static lv_obj_t *pagerMenuButtonAt(int index)
{
    switch (index) {
    case 0: return objects.home_button;
    case 1: return objects.nodes_button;
    case 2: return objects.groups_button;
    case 3: return objects.messages_button;
    case 4: return objects.map_button;
    case 5: return objects.settings_button;
    default: return nullptr;
    }
}

static bool pagerMenuButtonAvailable(lv_obj_t *button)
{
    return button && !lv_obj_has_flag(button, LV_OBJ_FLAG_HIDDEN) && !lv_obj_has_state(button, LV_STATE_DISABLED);
}

static bool pagerFocusTargetAvailable(lv_obj_t *obj)
{
    if (!obj || obj == objects.home_container)
        return false;

    uint32_t activeTabIndex = lv_tabview_get_tab_active(objects.controller_tab_view);
    lv_obj_t *tabBar = lv_tabview_get_tab_bar(objects.controller_tab_view);
    lv_obj_t *activeTabButton = lv_obj_get_child_by_type(tabBar, activeTabIndex, &lv_button_class);
    if (obj == activeTabButton)
        return false;

    lv_obj_t *inactiveTab = activeTabIndex == 0 ? objects.tab_page_tools : objects.tab_page_basic_settings;
    bool inScrollablePanel = false;
    for (lv_obj_t *parent = obj; parent; parent = lv_obj_get_parent(parent)) {
        if (parent == inactiveTab || lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN))
            return false;
        if (parent != obj && lv_obj_has_flag(parent, LV_OBJ_FLAG_SCROLLABLE))
            inScrollablePanel = true;
    }
    return lv_obj_is_visible(obj) ||
           (lv_obj_get_screen(obj) == lv_screen_active() && inScrollablePanel);
}

static void pagerFocusContentStep(lv_group_t *group, int direction)
{
    const uint32_t count = lv_group_get_obj_count(group);
    if (!count)
        return;

    int current = -1;
    lv_obj_t *focused = lv_group_get_focused(group);
    for (uint32_t i = 0; i < count; ++i) {
        if (lv_group_get_obj_by_index(group, i) == focused) {
            current = static_cast<int>(i);
            break;
        }
    }
    if (current < 0)
        current = direction > 0 ? -1 : 0;

    for (uint32_t step = 1; step <= count; ++step) {
        int index = (current + direction * static_cast<int>(step)) % static_cast<int>(count);
        if (index < 0)
            index += static_cast<int>(count);
        lv_obj_t *candidate = lv_group_get_obj_by_index(group, static_cast<uint32_t>(index));
        if (!candidate || (lv_obj_get_state(candidate) & LV_STATE_DISABLED) ||
            pagerMenuButtonIndex(candidate) >= 0 || !pagerFocusTargetAvailable(candidate))
            continue;
        lv_group_focus_obj(candidate);
        return;
    }
}

static void pagerFocusMenuStep(lv_group_t *group, int direction)
{
    int index = pagerMenuButtonIndex(lv_group_get_focused(group));
    if (index < 0)
        index = pagerMenuButtonIndex(pagerActiveMenuButton);
    if (index < 0)
        index = 0;

    for (int i = 0; i < 6; ++i) {
        index = (index + direction + 6) % 6;
        lv_obj_t *button = pagerMenuButtonAt(index);
        if (pagerMenuButtonAvailable(button)) {
            lv_group_focus_obj(button);
            return;
        }
    }
}

bool pagerHandleBack(lv_indev_t *indev)
{
    lv_group_t *group = indev ? lv_indev_get_group(indev) : nullptr;
    if (!group)
        return false;

    lv_obj_t *focused = lv_group_get_focused(group);
    lv_obj_t *screen = lv_screen_active();

    lv_obj_t *cancelButtons[] = {
        objects.obj2__cancel_button_w, objects.obj3__cancel_button_w, objects.obj4__cancel_button_w,
        objects.obj5__cancel_button_w, objects.obj6__cancel_button_w, objects.obj7__cancel_button_w,
        objects.obj8__cancel_button_w, objects.obj9__cancel_button_w, objects.obj10__cancel_button_w,
        objects.obj11__cancel_button_w, objects.obj12__cancel_button_w, objects.obj13__cancel_button_w,
        objects.obj14__cancel_button_w, objects.obj15__cancel_button_w, objects.obj16__cancel_button_w,
        objects.obj17__cancel_button_w, objects.obj18__cancel_button_w, objects.obj21__cancel_button_w,
        objects.obj27__cancel_button_w,
    };
    for (lv_obj_t *parent = focused; parent && parent != screen; parent = lv_obj_get_parent(parent)) {
        for (lv_obj_t *button : cancelButtons) {
            for (lv_obj_t *ancestor = button; ancestor && ancestor != screen; ancestor = lv_obj_get_parent(ancestor)) {
                if (lv_obj_has_flag(ancestor, LV_OBJ_FLAG_HIDDEN) || lv_obj_has_state(ancestor, LV_STATE_DISABLED))
                    break;
                if (ancestor == parent) {
                    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
                    return true;
                }
            }
        }
    }

    if (screen != objects.main_screen || !pagerMenuButtonAvailable(pagerActiveMenuButton) ||
        pagerMenuButtonIndex(focused) >= 0)
        return false;
    lv_group_set_editing(group, false);
    lv_group_focus_obj(pagerActiveMenuButton);
    return true;
}

static bool pagerMoveFocus(lv_group_t *group, int16_t diff)
{
    if (!group || !diff)
        return false;

    if (lv_screen_active() == objects.blank_screen)
        return true;

    lv_obj_t *scrollPanel = nullptr;
    if (!lv_obj_has_flag(objects.settings_about_panel, LV_OBJ_FLAG_HIDDEN))
        scrollPanel = objects.settings_about_panel;
    else if (!lv_obj_has_flag(objects.tools_statistics_panel, LV_OBJ_FLAG_HIDDEN))
        scrollPanel = objects.tools_statistics_panel;
    else if (!lv_obj_has_flag(objects.tools_packet_log_panel, LV_OBJ_FLAG_HIDDEN))
        scrollPanel = objects.tools_packet_log_panel;
    if (scrollPanel) {
        for (int16_t step = 0; step < abs(diff); ++step)
            lv_obj_scroll_by_bounded(scrollPanel, 0, diff > 0 ? -80 : 80, LV_ANIM_ON);
        return true;
    }

    if (pagerMoveMapFocus(group, diff))
        return true;

    if (pagerMoveNodeFocus(group, diff))
        return true;

    if (pagerMenuButtonIndex(lv_group_get_focused(group)) >= 0) {
        lv_group_set_editing(group, false);
        while (diff > 0) {
            pagerFocusMenuStep(group, 1);
            --diff;
        }
        while (diff < 0) {
            pagerFocusMenuStep(group, -1);
            ++diff;
        }
        return true;
    }

    if (lv_group_get_editing(group))
        return false;

    while (diff > 0) {
        pagerFocusContentStep(group, 1);
        --diff;
    }
    while (diff < 0) {
        pagerFocusContentStep(group, -1);
        ++diff;
    }
    return true;
}

class PagerRotaryInputDriver final : public InputDriver
{
  public:
    PagerRotaryInputDriver() : rotary(INPUTDRIVER_ROTARY_UP, INPUTDRIVER_ROTARY_DOWN, INPUTDRIVER_ROTARY_BTN)
    {
        rotaryInstance = &rotary;
    }

    void init() override
    {
        encoder = lv_indev_create();
        lv_indev_set_type(encoder, LV_INDEV_TYPE_ENCODER);
        lv_indev_set_read_cb(encoder, encoder_read);
        if (!inputGroup) {
            inputGroup = lv_group_create();
            lv_group_set_default(inputGroup);
        }
        lv_indev_set_group(encoder, inputGroup);
        attachInterrupt(INPUTDRIVER_ROTARY_UP, rotary_interrupt, CHANGE);
        attachInterrupt(INPUTDRIVER_ROTARY_DOWN, rotary_interrupt, CHANGE);
    }

  private:
    static void rotary_interrupt()
    {
        portENTER_CRITICAL_ISR(&rotaryLock);
        switch (rotaryInstance->process()) {
        case RotaryEncoder::DIRECTION_CW:
            if (encoderDiff < INT16_MAX)
                encoderDiff++;
            break;
        case RotaryEncoder::DIRECTION_CCW:
            if (encoderDiff > INT16_MIN)
                encoderDiff--;
            break;
        default:
            break;
        }
        portEXIT_CRITICAL_ISR(&rotaryLock);
    }

    static void encoder_read(lv_indev_t *indev, lv_indev_data_t *data)
    {
        const bool buttonPressed = rotaryInstance && rotaryInstance->readButton() == RotaryEncoder::BUTTON_PRESSED;
        lv_group_t *group = indev ? lv_indev_get_group(indev) : nullptr;
        if (buttonPressed && !suppressEncoderClick && pagerFunctionModifierActive) {
            pagerClearFunctionModifier();
            if (!pagerHandleBack(indev) && group) {
                lv_group_send_data(group, LV_KEY_ESC);
                lv_obj_t *target = lv_group_get_focused(group);
                if (target)
                    lv_obj_send_event(target, LV_EVENT_CANCEL, indev);
            }
            suppressEncoderClick = true;
        }
        lv_obj_t *focused = group ? lv_group_get_focused(group) : nullptr;
        if (buttonPressed && !suppressEncoderClick && group && focused && lv_obj_check_type(focused, &lv_textarea_class)) {
            lv_group_set_editing(group, false);
            lv_group_focus_next(group);
            suppressEncoderClick = true;
        }

        portENTER_CRITICAL(&rotaryLock);
        const int16_t diff = encoderDiff;
        encoderDiff = 0;
        portEXIT_CRITICAL(&rotaryLock);
        if (diff || buttonPressed)
            lv_display_trigger_activity(lv_indev_get_display(indev));
        data->key = suppressEncoderClick ? 0 : LV_KEY_ENTER;
        data->enc_diff = pagerMoveFocus(group, diff) ? 0 : diff;
        data->state = buttonPressed && !suppressEncoderClick ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        if (!buttonPressed)
            suppressEncoderClick = false;
    }

    RotaryEncoder rotary;
    inline static RotaryEncoder *rotaryInstance = nullptr;
    inline static int16_t encoderDiff = 0;
    inline static portMUX_TYPE rotaryLock = portMUX_INITIALIZER_UNLOCKED;
    inline static bool suppressEncoderClick = false;
};
#endif

#if defined(INPUTDRIVER_ROTARY_TYPE)
static PagerRotaryInputDriver *pagerRotaryDriver = nullptr;
#endif

#include "util/ILog.h"

DeviceGUI::DeviceGUI(const DisplayDriverConfig *cfg, DisplayDriver *driver) : displaydriver(driver), inputdriver(nullptr)
{

#if LV_USE_LIBINPUT
    if (cfg) {
        linuxInputDriver = new LinuxInputDriver(cfg->keyboard(), cfg->pointer(), cfg->keyboardNavigation(), cfg->cursorMode());
        inputdriver = linuxInputDriver;
    }
#endif
#if defined(INPUTDRIVER_ENCODER_TYPE)
    encoderDriver = new EncoderInputDriver;
#endif
#if defined(INPUTDRIVER_MATRIX_TYPE)
    keyMatrixDriver = new KeyMatrixInputDriver;
#endif
#if defined(INPUTDRIVER_BUTTON_TYPE)
    buttonDriver = new ButtonInputDriver;
#endif
#if defined(INPUTDRIVER_DIRECTIONALPAD_TYPE)
    padDriver = new DirectionalPadInputDriver;
#endif

#if defined(INPUTDRIVER_ROTARY_TYPE)
    pagerRotaryDriver = new PagerRotaryInputDriver;
#endif

    if (!inputdriver)
        inputdriver = InputDriver::instance();
}

void DeviceGUI::init(IClientBase *client)
{
    ILOG_DEBUG("Display driver init...");
    displaydriver->init(this);

    ILOG_DEBUG("Input driver init...");
    I2CKeyboardScanner scanner;
    keyboardDriver = scanner.scan();
    if (keyboardDriver)
        keyboardDriver->init();
#if LV_USE_LIBINPUT
    if (linuxInputDriver)
        linuxInputDriver->init();
#endif
#if defined(INPUTDRIVER_ENCODER_TYPE)
    if (encoderDriver)
        encoderDriver->init();
#endif
#if defined(INPUTDRIVER_MATRIX_TYPE)
    if (keyMatrixDriver)
        keyMatrixDriver->init();
#endif
#if defined(INPUTDRIVER_BUTTON_TYPE)
    if (buttonDriver)
        buttonDriver->init();
#endif
#if defined(INPUTDRIVER_DIRECTIONALPAD_TYPE)
    if (padDriver)
        padDriver->init();
#endif

#if defined(INPUTDRIVER_ROTARY_TYPE)
    if (pagerRotaryDriver)
        pagerRotaryDriver->init();
#endif

    displaydriver->printConfig();
}

void DeviceGUI::toggleDisplay(void)
{
    displaydriver->toggleDisplay();
}

void DeviceGUI::task_handler(void)
{
    displaydriver->task_handler();
#if defined(INPUTDRIVER_ROTARY_TYPE)
    if (pagerRotaryDriver)
        pagerRotaryDriver->task_handler();
#endif
}

DeviceGUI::~DeviceGUI()
{
    delete inputdriver;
}

void DeviceGUI::prepareSleep(void)
{
    if (inputdriver)
        inputdriver->prepareSleep();
}

void DeviceGUI::wakeUp(void)
{
    if (inputdriver)
        inputdriver->wakeUp();
}
