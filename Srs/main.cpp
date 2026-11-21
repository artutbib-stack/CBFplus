#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <string>

using namespace geode::prelude;

struct InputBufferState {
    bool enabled = true;
    bool soundEnabled = true;
    int bufferTicks = 10;

    int bufferedTicksLeft = 0;
    bool awaitingBufferedJump = false;
    bool physButtonHeld = false;
    bool bufferedActionExecuted = false;

    void clearAttemptState(PlayerObject* p1 = nullptr, PlayerObject* p2 = nullptr) {
        bufferedTicksLeft = 0;
        awaitingBufferedJump = false;
        bufferedActionExecuted = false;
        if (!physButtonHeld) {
            if (p1) {
                p1->releaseButton(PlayerButton::Jump);
            }
            if (p2) {
                p2->releaseButton(PlayerButton::Jump);
            }
        }
    }

    void fullReset() {
        bufferedTicksLeft = 0;
        awaitingBufferedJump = false;
        physButtonHeld = false;
        bufferedActionExecuted = false;
    }
};

static InputBufferState g_state;
static bool g_menuOpen = false;

static void playBufferTriggerSound() {
    if (!g_state.enabled || !g_state.soundEnabled) return;
    auto engine = FMODAudioEngine::sharedEngine();
    if (!engine) return;

    engine->playEffect("achievement_01.ogg");
}

static bool canExecuteBufferedPress(PlayerObject* player) {
    if (!player) return false;
    return player->m_isOnGround ||
           player->m_isShip ||
           player->m_isBird ||
           player->m_isDart ||
           player->m_isSwing;
}

class MainModMenuPopup;

static void openBufferMenu();

class CBFSettingsPopup : public geode::Popup {
protected:
    TextInput* m_tickInput = nullptr;

    bool init() override {
        if (!Popup::init(275.f, 175.f)) return false;

        this->setTitle("Input Buffer Settings");
        auto winSize = m_mainLayer->getContentSize();

        auto desc = CCLabelBMFont::create("Buffer Window (0 - 100 ticks):", "bigFont.fnt");
        desc->setScale(0.36f);
        desc->setPosition({ winSize.width / 2, winSize.height / 2 + 32.f });
        m_mainLayer->addChild(desc);

        auto subDesc = CCLabelBMFont::create("Early press stays active up to N ticks", "goldFont.fnt");
        subDesc->setScale(0.29f);
        subDesc->setPosition({ winSize.width / 2, winSize.height / 2 + 15.f });
        m_mainLayer->addChild(subDesc);

        m_tickInput = TextInput::create(110.f, "Ticks", "bigFont.fnt");
        m_tickInput->setCommonFilter(CommonFilter::Uint);
        m_tickInput->setMaxCharCount(3);
        m_tickInput->setString(std::to_string(g_state.bufferTicks));
        m_tickInput->setPosition({ winSize.width / 2, winSize.height / 2 - 12.f });
        m_mainLayer->addChild(m_tickInput);

        auto saveBtnSpr = ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto saveBtn = CCMenuItemExt::createSpriteExtra(saveBtnSpr, [this](CCObject*) {
            int parsed = geode::utils::numFromString<int>(m_tickInput->getString()).unwrapOr(10);
            g_state.bufferTicks = std::clamp(parsed, 0, 100);
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
    CCMenuItemToggler* m_enableToggle = nullptr;
    CCMenuItemToggler* m_soundToggle = nullptr;

    bool init() override {
        if (!Popup::init(325.f, 185.f)) return false;

        g_menuOpen = true;
        this->setTitle("Input Buffer");
        auto winSize = m_mainLayer->getContentSize();
        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(menu);

        m_enableToggle = CCMenuItemExt::createTogglerWithStandardSprites(0.75f, [this](CCMenuItemToggler* sender) {
            bool nextEnabled = !sender->isToggled();
            g_state.enabled = nextEnabled;
            Mod::get()->setSavedValue<bool>("cbf-enabled", g_state.enabled);
            if (!g_state.enabled) {
                g_state.fullReset();
            }
        });
        m_enableToggle->toggle(g_state.enabled);
        m_enableToggle->setPosition({ 44.f, winSize.height / 2 + 28.f });
        menu->addChild(m_enableToggle);

        auto enableLabelSpr = CCLabelBMFont::create("Enable Buffer", "bigFont.fnt");
        enableLabelSpr->setScale(0.42f);
        auto ticksBtn = CCMenuItemExt::createSpriteExtra(enableLabelSpr, [](CCObject*) {
            CBFSettingsPopup::create()->show();
        });
        ticksBtn->setAnchorPoint({ 0.0f, 0.5f });
        ticksBtn->setPosition({ 70.f, winSize.height / 2 + 28.f });
        menu->addChild(ticksBtn);

        auto hintText = fmt::format("[Window: {} ticks - Tap text to edit]", g_state.bufferTicks);
        auto hintSpr = CCLabelBMFont::create(hintText.c_str(), "goldFont.fnt");
        hintSpr->setScale(0.28f);
        hintSpr->setAnchorPoint({ 0.0f, 0.5f });
        hintSpr->setPosition({ 70.f, winSize.height / 2 + 11.f });
        m_mainLayer->addChild(hintSpr);

        m_soundToggle = CCMenuItemExt::createTogglerWithStandardSprites(0.75f, [](CCMenuItemToggler* sender) {
            bool nextSound = !sender->isToggled();
            g_state.soundEnabled = nextSound;
            Mod::get()->setSavedValue<bool>("cbf-sound-enabled", g_state.soundEnabled);
            if (g_state.soundEnabled) {
                playBufferTriggerSound();
            }
        });
        m_soundToggle->toggle(g_state.soundEnabled);
        m_soundToggle->setPosition({ 44.f, winSize.height / 2 - 22.f });
        menu->addChild(m_soundToggle);

        auto soundLabelSpr = CCLabelBMFont::create("Trigger Sound", "bigFont.fnt");
        soundLabelSpr->setScale(0.40f);
        soundLabelSpr->setAnchorPoint({ 0.0f, 0.5f });
        soundLabelSpr->setPosition({ 70.f, winSize.height / 2 - 22.f });
        m_mainLayer->addChild(soundLabelSpr);

        auto soundSubSpr = CCLabelBMFont::create("Play sound when buffered press fires", "goldFont.fnt");
        soundSubSpr->setScale(0.27f);
        soundSubSpr->setAnchorPoint({ 0.0f, 0.5f });
        soundSubSpr->setPosition({ 70.f, winSize.height / 2 - 38.f });
        m_mainLayer->addChild(soundSubSpr);

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

static void openBufferMenu() {
    if (!g_menuOpen) {
        if (auto popup = MainModMenuPopup::create()) {
            popup->show();
        }
    }
}

#ifndef GEODE_IS_MOBILE
class $modify(CBFKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isRepeat, double timestamp) {
        if (down && !isRepeat && key == KEY_F && this->getControlKeyPressed()) {
            openBufferMenu();
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isRepeat, timestamp);
    }
};
#endif

class $modify(CBFPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) {
            return false;
        }
        g_state.fullReset();

#ifdef GEODE_IS_MOBILE
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto overlayMenu = CCMenu::create();
        overlayMenu->setPosition({ 0.f, 0.f });
        overlayMenu->setZOrder(120);

        auto iconSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        if (!iconSpr) {
            iconSpr = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
        }
        auto circleSpr = CircleButtonSprite::create(
            iconSpr,
            CircleBaseColor::Green,
            CircleBaseSize::Small
        );
        auto openBtn = CCMenuItemExt::createSpriteExtra(circleSpr, [](CCObject*) {
            openBufferMenu();
        });
        openBtn->setPosition({ 38.f, winSize.height - 38.f });

        overlayMenu->addChild(openBtn);
        this->addChild(overlayMenu);
#endif

        return true;
    }

    void resetLevel() {
        g_state.clearAttemptState(m_player1, m_player2);
        PlayLayer::resetLevel();
    }

    void onQuit() {
        g_state.fullReset();
        PlayLayer::onQuit();
    }
};

class $modify(CBFPlayerObject, PlayerObject) {
    void pushButton(PlayerButton btn) {
        if (btn == PlayerButton::Jump) {
            g_state.physButtonHeld = true;
            g_state.bufferedActionExecuted = false;

            if (g_state.enabled && g_state.bufferTicks > 0 && !canExecuteBufferedPress(this)) {
                g_state.awaitingBufferedJump = true;
                g_state.bufferedTicksLeft = g_state.bufferTicks;
                return;
            }

            g_state.awaitingBufferedJump = false;
            g_state.bufferedTicksLeft = 0;
        }

        PlayerObject::pushButton(btn);
    }

    void releaseButton(PlayerButton btn) {
        if (btn == PlayerButton::Jump) {
            g_state.physButtonHeld = false;
            g_state.awaitingBufferedJump = false;
            g_state.bufferedTicksLeft = 0;
            g_state.bufferedActionExecuted = false;
        }

        PlayerObject::releaseButton(btn);
    }

    void update(float dt) {
        PlayerObject::update(dt);

        if (!g_state.enabled || !g_state.awaitingBufferedJump) {
            return;
        }

        if (this->m_isDead) {
            g_state.clearAttemptState(this, nullptr);
            return;
        }

        if (!g_state.physButtonHeld) {
            g_state.awaitingBufferedJump = false;
            g_state.bufferedTicksLeft = 0;
            return;
        }

        if (g_state.bufferedTicksLeft > 0) {
            if (!g_state.bufferedActionExecuted && canExecuteBufferedPress(this)) {
                g_state.awaitingBufferedJump = false;
                g_state.bufferedTicksLeft = 0;
                g_state.bufferedActionExecuted = true;
                PlayerObject::pushButton(PlayerButton::Jump);
                playBufferTriggerSound();
                return;
            }

            g_state.bufferedTicksLeft--;
            if (g_state.bufferedTicksLeft <= 0) {
                g_state.awaitingBufferedJump = false;
                g_state.bufferedTicksLeft = 0;
            }
        } else {
            g_state.awaitingBufferedJump = false;
        }
    }
};

$on_mod(Loaded) {
    g_state.enabled = Mod::get()->getSavedValue<bool>("cbf-enabled", true);
    g_state.soundEnabled = Mod::get()->getSavedValue<bool>("cbf-sound-enabled", true);
    g_state.bufferTicks = static_cast<int>(Mod::get()->getSavedValue<int64_t>("cbf-ticks", 10));
};
