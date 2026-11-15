#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>

using namespace geode::prelude;

struct ModState {
    bool cbfEnabled = true;
    int bufferTicks = 4;

    bool spamBufferEnabled = false;
    int holdTicks = 4;
    int releaseTicks = 4;

    int remainingCbfTicks = 0;
    bool isHolding = false;
    int spamTickCounter = 0;
    bool spamIsPressed = false;

    CCPoint buttonPos = { 45.0f, 160.0f };
};

static ModState g_state;

class CBFSettingsPopup : public geode::Popup<> {
protected:
    TextInput* m_tickInput = nullptr;

    bool setup() override {
        this->setTitle("CBF Configuration");
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
                fmt::format("CBF set to {} ticks", g_state.bufferTicks),
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
        if (ret->initAnchored(265.f, 165.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class SpamSettingsPopup : public geode::Popup<> {
protected:
    TextInput* m_holdInput = nullptr;
    TextInput* m_releaseInput = nullptr;

    bool setup() override {
        this->setTitle("Spam Buffer (Ticks)");
        auto winSize = m_mainLayer->getContentSize();
        auto menu = CCMenu::create();
        menu->setPosition({ 0, 0 });
        m_mainLayer->addChild(menu);

        auto hintLbl = CCLabelBMFont::create("240 TPS (e.g. 4 Hold / 4 Release = 1-block wave)", "goldFont.fnt");
        hintLbl->setScale(0.30f);
        hintLbl->setPosition({ winSize.width / 2, winSize.height / 2 + 38.f });
        m_mainLayer->addChild(hintLbl);

        auto holdLbl = CCLabelBMFont::create("Hold Ticks (1-100):", "bigFont.fnt");
        holdLbl->setScale(0.38f);
        holdLbl->setAnchorPoint({ 0.0f, 0.5f });
        holdLbl->setPosition({ 30.f, winSize.height / 2 + 10.f });
        m_mainLayer->addChild(holdLbl);

        m_holdInput = TextInput::create(75.f, "Hold", "bigFont.fnt");
        m_holdInput->setCommonFilter(CommonFilter::Uint);
        m_holdInput->setMaxCharCount(3);
        m_holdInput->setString(std::to_string(g_state.holdTicks));
        m_holdInput->setPosition({ winSize.width - 65.f, winSize.height / 2 + 10.f });
        m_mainLayer->addChild(m_holdInput);

        auto releaseLbl = CCLabelBMFont::create("Release Ticks (1-100):", "bigFont.fnt");
        releaseLbl->setScale(0.38f);
        releaseLbl->setAnchorPoint({ 0.0f, 0.5f });
        releaseLbl->setPosition({ 30.f, winSize.height / 2 - 22.f });
        m_mainLayer->addChild(releaseLbl);

        m_releaseInput = TextInput::create(75.f, "Release", "bigFont.fnt");
        m_releaseInput->setCommonFilter(CommonFilter::Uint);
        m_releaseInput->setMaxCharCount(3);
        m_releaseInput->setString(std::to_string(g_state.releaseTicks));
        m_releaseInput->setPosition({ winSize.width - 65.f, winSize.height / 2 - 22.f });
        m_mainLayer->addChild(m_releaseInput);

        auto saveBtnSpr = ButtonSprite::create("Apply", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto saveBtn = CCMenuItemExt::createSpriteExtra(saveBtnSpr, [this](CCObject*) {
            int parsedHold = geode::utils::numFromString<int>(m_holdInput->getString()).unwrapOr(4);
            int parsedRelease = geode::utils::numFromString<int>(m_releaseInput->getString()).unwrapOr(4);

            g_state.holdTicks = std::clamp(parsedHold, 1, 100);
            g_state.releaseTicks = std::clamp(parsedRelease, 1, 100);

            Mod::get()->setSavedValue<int64_t>("spam-hold-ticks", g_state.holdTicks);
            Mod::get()->setSavedValue<int64_t>("spam-release-ticks", g_state.releaseTicks);

            Notification::create(
                fmt::format("Spam: {} Hold / {} Release ticks", g_state.holdTicks, g_state.releaseTicks),
                NotificationIcon::Success
            )->show();
            this->onClose(nullptr);
        });
        saveBtn->setPosition({ winSize.width / 2, 26.f });
        menu->addChild(saveBtn);

        return true;
    }

public:
    static SpamSettingsPopup* create() {
        auto ret = new SpamSettingsPopup();
        if (ret->initAnchored(295.f, 190.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class MainModMenuPopup : public geode::Popup<> {
protected:
    CCMenuItemToggler* m_cbfToggle = nullptr;
    CCMenuItemToggler* m_spamToggle = nullptr;

    bool setup() override {
        this->setTitle("ClickBeforeFrames");
        auto winSize = m_mainLayer->getContentSize();
        auto menu = CCMenu::create();
        menu->setPosition({ 0, 0 });
        m_mainLayer->addChild(menu);

        m_cbfToggle = CCMenuItemExt::createTogglerWithStandardSprites(0.75f, [this](CCMenuItemToggler* sender) {
            g_state.cbfEnabled = !sender->isToggled();
            Mod::get()->setSavedValue<bool>("cbf-enabled", g_state.cbfEnabled);
        });
        m_cbfToggle->toggle(g_state.cbfEnabled);
        m_cbfToggle->setPosition({ 45.f, winSize.height / 2 + 24.f });
        menu->addChild(m_cbfToggle);

        auto cbfLabelSpr = CCLabelBMFont::create("CBF (Click Before Frames)", "bigFont.fnt");
        cbfLabelSpr->setScale(0.45f);
        auto cbfSubBtn = CCMenuItemExt::createSpriteExtra(cbfLabelSpr, [this](CCObject*) {
            CBFSettingsPopup::create()->show();
        });
        cbfSubBtn->setAnchorPoint({ 0.0f, 0.5f });
        cbfSubBtn->setPosition({ 68.f, winSize.height / 2 + 24.f });
        menu->addChild(cbfSubBtn);

        auto cbfHint = CCLabelBMFont::create("[Tap text to set buffer ticks]", "goldFont.fnt");
        cbfHint->setScale(0.28f);
        cbfHint->setAnchorPoint({ 0.0f, 0.5f });
        cbfHint->setPosition({ 68.f, winSize.height / 2 + 10.f });
        m_mainLayer->addChild(cbfHint);

        m_spamToggle = CCMenuItemExt::createTogglerWithStandardSprites(0.75f, [this](CCMenuItemToggler* sender) {
            g_state.spamBufferEnabled = !sender->isToggled();
            Mod::get()->setSavedValue<bool>("spam-enabled", g_state.spamBufferEnabled);
        });
        m_spamToggle->toggle(g_state.spamBufferEnabled);
        m_spamToggle->setPosition({ 45.f, winSize.height / 2 - 22.f });
        menu->addChild(m_spamToggle);

        auto spamLabelSpr = CCLabelBMFont::create("Spam Buffer", "bigFont.fnt");
        spamLabelSpr->setScale(0.45f);
        auto spamSubBtn = CCMenuItemExt::createSpriteExtra(spamLabelSpr, [this](CCObject*) {
            SpamSettingsPopup::create()->show();
        });
        spamSubBtn->setAnchorPoint({ 0.0f, 0.5f });
        spamSubBtn->setPosition({ 68.f, winSize.height / 2 - 22.f });
        menu->addChild(spamSubBtn);

        auto spamHint = CCLabelBMFont::create("[Tap text to set Hold/Release ticks]", "goldFont.fnt");
        spamHint->setScale(0.28f);
        spamHint->setAnchorPoint({ 0.0f, 0.5f });
        spamHint->setPosition({ 68.f, winSize.height / 2 - 36.f });
        m_mainLayer->addChild(spamHint);

        return true;
    }

public:
    static MainModMenuPopup* create() {
        auto ret = new MainModMenuPopup();
        if (ret->initAnchored(315.f, 180.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class DraggableCBFButton : public CCLayer {
protected:
    CCNode* m_visualBtn = nullptr;
    bool m_isDragging = false;
    bool m_hasMoved = false;
    CCPoint m_touchStartPos;
    CCPoint m_btnStartPos;

    bool init() override {
        if (!CCLayer::init()) return false;

        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(-500);

        auto bg = CCScale9Sprite::create("GJ_button_01.png");
        bg->setContentSize({ 46.f, 26.f });

        auto lbl = CCLabelBMFont::create("CBF", "bigFont.fnt");
        lbl->setScale(0.48f);
        lbl->setPosition(bg->getContentSize() / 2);
        bg->addChild(lbl);

        m_visualBtn = bg;
        m_visualBtn->setPosition(g_state.buttonPos);
        this->addChild(m_visualBtn);

        return true;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        auto pos = touch->getLocation();
        if (m_visualBtn && m_visualBtn->boundingBox().containsPoint(this->convertToNodeSpace(pos))) {
            m_isDragging = true;
            m_hasMoved = false;
            m_touchStartPos = pos;
            m_btnStartPos = m_visualBtn->getPosition();
            return true;
        }
        return false;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (!m_isDragging || !m_visualBtn) return;
        auto pos = touch->getLocation();
        auto diff = pos - m_touchStartPos;
        if (diff.getLength() > 5.0f) {
            m_hasMoved = true;
        }
        auto newPos = m_btnStartPos + diff;
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        newPos.x = std::clamp(newPos.x, 25.f, winSize.width - 25.f);
        newPos.y = std::clamp(newPos.y, 25.f, winSize.height - 25.f);
        
        m_visualBtn->setPosition(newPos);
        g_state.buttonPos = newPos;
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        if (m_isDragging && !m_hasMoved) {
            MainModMenuPopup::create()->show();
        }
        m_isDragging = false;
    }

public:
    static DraggableCBFButton* create() {
        auto ret = new DraggableCBFButton();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(CBFPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) {
            return false;
        }

        if (auto hud = this->getChildByID("ui-layer")) {
            hud->addChild(DraggableCBFButton::create(), 100);
        } else {
            this->addChild(DraggableCBFButton::create(), 100);
        }

        g_state.remainingCbfTicks = 0;
        g_state.isHolding = false;
        g_state.spamTickCounter = 0;
        g_state.spamIsPressed = false;

        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!g_state.cbfEnabled || !m_player1) return;

        if (g_state.remainingCbfTicks > 0) {
            g_state.remainingCbfTicks--;
            if (g_state.remainingCbfTicks == 0 && g_state.isHolding) {
                m_player1->releaseButton(PlayerButton::Jump);
                g_state.isHolding = false;
            }
        }

        if (g_state.spamBufferEnabled && g_state.spamIsPressed) {
            g_state.spamTickCounter--;
            if (g_state.spamTickCounter <= 0) {
                if (m_player1->m_isButton1) {
                    m_player1->releaseButton(PlayerButton::Jump);
                    g_state.spamTickCounter = g_state.releaseTicks;
                } else {
                    m_player1->pushButton(PlayerButton::Jump);
                    g_state.spamTickCounter = g_state.holdTicks;
                }
            }
        }
    }
};

class $modify(CBFPlayerObject, PlayerObject) {
    void pushButton(PlayerButton btn) {
        PlayerObject::pushButton(btn);
        if (g_state.cbfEnabled && btn == PlayerButton::Jump) {
            g_state.isHolding = true;
            g_state.remainingCbfTicks = g_state.bufferTicks;
        }
        if (g_state.spamBufferEnabled && btn == PlayerButton::Jump) {
            g_state.spamIsPressed = true;
            g_state.spamTickCounter = g_state.holdTicks;
        }
    }

    void releaseButton(PlayerButton btn) {
        if (g_state.cbfEnabled && btn == PlayerButton::Jump && g_state.remainingCbfTicks > 0) {
            return;
        }
        PlayerObject::releaseButton(btn);
        if (btn == PlayerButton::Jump) {
            g_state.isHolding = false;
            g_state.spamIsPressed = false;
        }
    }
};

$on_mod(Loaded) {
    g_state.cbfEnabled = Mod::get()->getSavedValue<bool>("cbf-enabled", true);
    g_state.bufferTicks = static_cast<int>(Mod::get()->getSavedValue<int64_t>("cbf-ticks", 4));
    g_state.spamBufferEnabled = Mod::get()->getSavedValue<bool>("spam-enabled", false);
    g_state.holdTicks = static_cast<int>(Mod::get()->getSavedValue<int64_t>("spam-hold-ticks", 4));
    g_state.releaseTicks = static_cast<int>(Mod::get()->getSavedValue<int64_t>("spam-release-ticks", 4));
};
