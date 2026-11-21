#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>

using namespace geode::prelude;

struct ModState {
    bool cbfEnabled = true;
    int bufferTicks = 4;

    int remainingCbfTicks = 0;
    bool physicallyHeld = false;
    bool isBufferedHold = false;

    void resetInput() {
        remainingCbfTicks = 0;
        physicallyHeld = false;
        isBufferedHold = false;
    }
};

static ModState g_state;
static bool g_menuOpen = false;

class CBFSettingsPopup : public geode::Popup {
protected:
    TextInput* m_tickInput = nullptr;

    bool init() override {
        if (!Popup::init(265.f, 165.f)) return false;

        this->setTitle("Input Buffer Settings");
        auto winSize = m_mainLayer->getContentSize();

        auto desc = CCLabelBMFont::create("Input Buffer Window (1 - 100 ticks):", "bigFont.fnt");
        desc->setScale(0.38f);
        desc->setPosition({ winSize.width / 2, winSize.height / 2 + 28.f });
        m_mainLayer->addChild(desc);

        auto subDesc = CCLabelBMFont::create("(240 TPS physics: 4 ticks = ~16.6ms)", "goldFont.fnt");
        subDesc->setScale(0.32f);
        subDesc->setPosition({ winSize.width / 2, winSize.height / 2 + 12.f });
        m_mainLayer->addChild(subDesc);

        m_tickInput = TextInput::create(100.f, "Ticks", "bigFont.fnt");
        m_tickInput->setCommonFilter(CommonFilter::Uint);
        m_tickInput->setMaxCharCount(3);
        m_tickInput->setString(std::to_string(g_state.bufferTicks));
        m_tickInput->setPosition({ winSize.width / 2, winSize.height / 2 - 14.f });
        m_mainLayer->addChild(m_tickInput);

        auto saveBtnSpr = ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto saveBtn = CCMenuItemExt::createSpriteExtra(saveBtnSpr, [this](CCObject*) {
            int parsed = geode::utils::numFromString<int>(m_tickInput->getString()).unwrapOr(4);
            g_state.bufferTicks = std::clamp(parsed, 1, 100);
            Mod::get()->setSavedValue<int64_t>("cbf-ticks", g_state.bufferTicks);
            Notification::create(
                fmt::format("Input Buffer: {} ticks", g_state.bufferTicks),
                NotificationIcon::Success
            )->show();
            this->onClose(nullptr);
        });

        auto menu = CCMenu::create();
        menu->setPosition({ winSize.width / 2, 28.f });
        menu->addChild(saveBtn);
        m_mainLayer->addChild(menu);

        return true;
    }

public:
    static CBFSettingsPopup* create() {
        auto ret = new CBFSettingsPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class MainModMenuPopup : public geode::Popup {
protected:
    CCMenuItemToggler* m_cbfToggle = nullptr;

    bool init() override {
        if (!Popup::init(305.f, 145.f)) return false;

        g_menuOpen = true;
        this->setTitle("Input Buffer");
        auto winSize = m_mainLayer->getContentSize();
        auto menu = CCMenu::create();
        menu->setPosition({ 0, 0 });
        m_mainLayer->addChild(menu);

        m_cbfToggle = CCMenuItemExt::createTogglerWithStandardSprites(0.8f, [this](CCMenuItemToggler* sender) {
            g_state.cbfEnabled = !sender->isToggled();
            Mod::get()->setSavedValue<bool>("cbf-enabled", g_state.cbfEnabled);
            if (!g_state.cbfEnabled) {
                g_state.resetInput();
            }
        });
        m_cbfToggle->toggle(g_state.cbfEnabled);
        m_cbfToggle->setPosition({ 48.f, winSize.height / 2 + 6.f });
        menu->addChild(m_cbfToggle);

        auto cbfLabelSpr = CCLabelBMFont::create("Enable Input Buffer", "bigFont.fnt");
        cbfLabelSpr->setScale(0.46f);
        auto cbfSubBtn = CCMenuItemExt::createSpriteExtra(cbfLabelSpr, [this](CCObject*) {
            CBFSettingsPopup::create()->show();
        });
        cbfSubBtn->setAnchorPoint({ 0.0f, 0.5f });
        cbfSubBtn->setPosition({ 74.f, winSize.height / 2 + 6.f });
        menu->addChild(cbfSubBtn);

        auto cbfHint = CCLabelBMFont::create("[Tap text to set buffer ticks]", "goldFont.fnt");
        cbfHint->setScale(0.30f);
        cbfHint->setAnchorPoint({ 0.0f, 0.5f });
        cbfHint->setPosition({ 74.f, winSize.height / 2 - 10.f });
        m_mainLayer->addChild(cbfHint);

        return true;
    }

    void onExit() override {
        g_menuOpen = false;
        Popup::onExit();
    }

public:
    static MainModMenuPopup* create() {
        auto ret = new MainModMenuPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(CBFKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isRepeat, double timestamp) {
        if (down && !isRepeat && key == KEY_F && this->getControlKeyPressed()) {
            if (!g_menuOpen) {
                if (auto popup = MainModMenuPopup::create()) {
                    popup->show();
                }
            }
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isRepeat, timestamp);
    }
};

class $modify(CBFPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) {
            return false;
        }
        g_state.resetInput();
        return true;
    }

    void resetLevel() {
        g_state.resetInput();
        PlayLayer::resetLevel();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!g_state.cbfEnabled || !m_player1) return;

        if (g_state.remainingCbfTicks > 0) {
            g_state.remainingCbfTicks--;
            if (g_state.remainingCbfTicks == 0 && g_state.isBufferedHold) {
                g_state.isBufferedHold = false;
                if (!g_state.physicallyHeld) {
                    m_player1->releaseButton(PlayerButton::Jump);
                }
            }
        }
    }
};

class $modify(CBFPlayerObject, PlayerObject) {
    void pushButton(PlayerButton btn) {
        PlayerObject::pushButton(btn);
        if (g_state.cbfEnabled && btn == PlayerButton::Jump) {
            g_state.physicallyHeld = true;
            g_state.isBufferedHold = true;
            g_state.remainingCbfTicks = g_state.bufferTicks;
        }
    }

    void releaseButton(PlayerButton btn) {
        if (g_state.cbfEnabled && btn == PlayerButton::Jump) {
            g_state.physicallyHeld = false;
            if (g_state.remainingCbfTicks > 0 && g_state.isBufferedHold) {
                return;
            }
        }
        PlayerObject::releaseButton(btn);
        if (btn == PlayerButton::Jump) {
            g_state.isBufferedHold = false;
        }
    }
};

$on_mod(Loaded) {
    g_state.cbfEnabled = Mod::get()->getSavedValue<bool>("cbf-enabled", true);
    g_state.bufferTicks = static_cast<int>(Mod::get()->getSavedValue<int64_t>("cbf-ticks", 4));
};
