#include <Geode/Geode.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <cmath>
#include <dlfcn.h>
#include <string>

using namespace geode::prelude;

namespace tea::mathlab {
    static thread_local bool g_bypassOverrides = false;

    bool hooksEnabled() {
        return Mod::get()->getSettingValue<bool>("global-math-hooks");
    }
    bool swapSinCos() {
        return Mod::get()->getSettingValue<bool>("swap-sin-cos");
    }
    double pi() {
        return static_cast<double>(Mod::get()->getSettingValue<float>("custom-pi"));
    }

    // Helpers also used by this mod. Global hooks below cover calls routed
    // through hooked libm symbols; compile-time constants/inlined math remain unchanged.
    double sin(double x) { return swapSinCos() ? std::cos(x) : std::sin(x); }
    double cos(double x) { return swapSinCos() ? std::sin(x) : std::cos(x); }

    struct BypassGuard {
        bool previous;
        BypassGuard() : previous(g_bypassOverrides) { g_bypassOverrides = true; }
        ~BypassGuard() { g_bypassOverrides = previous; }
    };

    // Geode routes calls to the original implementation when a hooked symbol
    // is called from a detour on the same thread. The bypass flag prevents the
    // second math function in a swap from applying the override a second time.
    double detourSin(double x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::sin(x);
        if (!swapSinCos()) return ::sin(x);
        BypassGuard guard;
        return ::cos(x);
    }
    double detourCos(double x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::cos(x);
        if (!swapSinCos()) return ::cos(x);
        BypassGuard guard;
        return ::sin(x);
    }
    float detourSinf(float x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::sinf(x);
        if (!swapSinCos()) return ::sinf(x);
        BypassGuard guard;
        return ::cosf(x);
    }
    float detourCosf(float x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::cosf(x);
        if (!swapSinCos()) return ::cosf(x);
        BypassGuard guard;
        return ::sinf(x);
    }
    double detourAcos(double x) {
        if (hooksEnabled() && !g_bypassOverrides && x == -1.0) return pi();
        return ::acos(x);
    }
    float detourAcosf(float x) {
        if (hooksEnabled() && !g_bypassOverrides && x == -1.0f)
            return static_cast<float>(pi());
        return ::acosf(x);
    }
    double detourAtan2(double y, double x) {
        if (hooksEnabled() && !g_bypassOverrides && x < 0.0 && y == 0.0)
            return std::signbit(y) ? -pi() : pi();
        return ::atan2(y, x);
    }
    float detourAtan2f(float y, float x) {
        if (hooksEnabled() && !g_bypassOverrides && x < 0.0f && y == 0.0f) {
            float value = static_cast<float>(pi());
            return std::signbit(y) ? -value : value;
        }
        return ::atan2f(y, x);
    }

    template <class Detour>
    void installMathHook(char const* symbol, Detour detour) {
        auto address = dlsym(RTLD_DEFAULT, symbol);
        if (!address) {
            log::warn("Tea Math Lab: math symbol '{}' was not found; skipping hook", symbol);
            return;
        }
        auto result = Mod::get()->hook(
            address,
            detour,
            std::string("TeaMathLab::") + symbol,
            tulip::hook::TulipConvention::Cdecl
        );
        if (!result) {
            log::warn("Tea Math Lab: couldn't hook '{}': {}", symbol, result.unwrapErr());
            return;
        }
        log::info("Tea Math Lab: installed math hook for {}", symbol);
    }

    class MathLabPopup final : public Popup<> {
    protected:
        bool setup() override {
            this->setTitle("Math Lab");
            auto* hint = CCLabelBMFont::create("Game math overrides", "bigFont.fnt");
            hint->setScale(0.42f);
            hint->setPosition({m_size.width / 2.f, m_size.height - 35.f});
            m_mainLayer->addChild(hint);

            auto* globalLabel = CCLabelBMFont::create("Global math hooks", "bigFont.fnt");
            globalLabel->setScale(0.43f); globalLabel->setAnchorPoint({0.f, 0.5f});
            globalLabel->setPosition({24.f, m_size.height - 65.f});
            m_mainLayer->addChild(globalLabel);
            auto* globalToggle = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(hooksEnabled() ? "ON" : "OFF", 60, true, "goldFont.fnt", "GJ_button_04.png", 30.f, 0.7f),
                this, menu_selector(MathLabPopup::onToggleGlobal));
            globalToggle->setPosition({m_size.width - 42.f, m_size.height - 65.f});
            m_buttonMenu->addChild(globalToggle);

            auto* swapLabel = CCLabelBMFont::create("Swap sin and cos", "bigFont.fnt");
            swapLabel->setScale(0.43f); swapLabel->setAnchorPoint({0.f, 0.5f});
            swapLabel->setPosition({24.f, m_size.height - 98.f});
            m_mainLayer->addChild(swapLabel);
            auto* swapToggle = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(swapSinCos() ? "ON" : "OFF", 60, true, "goldFont.fnt", "GJ_button_04.png", 30.f, 0.7f),
                this, menu_selector(MathLabPopup::onToggleSwap));
            swapToggle->setPosition({m_size.width - 42.f, m_size.height - 98.f});
            m_buttonMenu->addChild(swapToggle);

            auto* piLabel = CCLabelBMFont::create("Custom pi value", "bigFont.fnt");
            piLabel->setScale(0.43f); piLabel->setAnchorPoint({0.f, 0.5f});
            piLabel->setPosition({24.f, m_size.height - 131.f});
            m_mainLayer->addChild(piLabel);
            m_piInput = TextInput::create(130.f, "Enter a number", "bigFont.fnt");
            m_piInput->setString(std::to_string(pi()), false);
            m_piInput->setFilter("0123456789.-");
            m_piInput->setPosition({m_size.width - 78.f, m_size.height - 131.f});
            m_mainLayer->addChild(m_piInput);

            auto* save = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png", 0.8f),
                this, menu_selector(MathLabPopup::onSave));
            save->setPosition({m_size.width / 2.f, 35.f});
            m_buttonMenu->addChild(save);

            auto* note = CCLabelBMFont::create("Best effort: not every math operation is hookable.", "bigFont.fnt");
            note->setScale(0.27f); note->setOpacity(190);
            note->setPosition({m_size.width / 2.f, 15.f});
            m_mainLayer->addChild(note);
            return true;
        }
        void reopen() { this->onClose(nullptr); MathLabPopup::create()->show(); }
        void onToggleGlobal(CCObject*) {
            Mod::get()->setSettingValue("global-math-hooks", !hooksEnabled());
            reopen();
        }
        void onToggleSwap(CCObject*) {
            Mod::get()->setSettingValue("swap-sin-cos", !swapSinCos());
            reopen();
        }
        void onSave(CCObject*) {
            if (!m_piInput) return;
            auto raw = m_piInput->getString();
            try {
                size_t used = 0;
                double value = std::stod(raw, &used);
                if (used != raw.size() || !std::isfinite(value) || value <= 0.0 || value > 1.0e12) {
                    FLAlertLayer::create("Math Lab", "Enter a finite number greater than 0 and at most 1e12.", "OK")->show();
                    return;
                }
                Mod::get()->setSettingValue("custom-pi", static_cast<float>(value));
                FLAlertLayer::create("Math Lab", "Custom pi saved.", "OK")->show();
            } catch (...) {
                FLAlertLayer::create("Math Lab", "That is not a valid number.", "OK")->show();
            }
        }
    public:
        static MathLabPopup* create() {
            auto* ret = new MathLabPopup();
            if (ret->init(290.f, 220.f, "GJ_square01.png")) { ret->autorelease(); return ret; }
            delete ret; return nullptr;
        }
    private:
        TextInput* m_piInput = nullptr;
    };
}

$on_mod(Loaded) {
    using namespace tea::mathlab;
    installMathHook("sin", &detourSin);
    installMathHook("cos", &detourCos);
    installMathHook("sinf", &detourSinf);
    installMathHook("cosf", &detourCosf);
    installMathHook("acos", &detourAcos);
    installMathHook("acosf", &detourAcosf);
    installMathHook("atan2", &detourAtan2);
    installMathHook("atan2f", &detourAtan2f);
}

class $modify(TeaMathOptionsLayer, OptionsLayer) {
    bool init() {
        if (!OptionsLayer::init()) return false;
        auto* button = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Math Lab", "goldFont.fnt", "GJ_button_04.png", 0.75f),
            this, menu_selector(TeaMathOptionsLayer::onOpenMathLab));
        auto* menu = CCMenu::create();
        menu->setID("tea-mathlab-menu"); menu->addChild(button);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width / 2.f, 34.f});
        this->addChild(menu);
        return true;
    }
    void onOpenMathLab(CCObject*) { tea::mathlab::MathLabPopup::create()->show(); }
};
