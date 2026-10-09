#include <Geode/Geode.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <cmath>
#include <cstdio>
#include <atomic>
#include <string>

#ifndef GEODE_IS_WINDOWS
    #include <dlfcn.h>
#endif

using namespace geode::prelude;

namespace woopet::mathfucker {
    static thread_local bool g_bypassOverrides = false;

    // Cached settings: the detours run on every sin/cos call, so they must not
    // touch Mod::getSettingValue (slow, and not safe from every thread).
    static std::atomic<bool> g_hooksEnabled{true};
    static std::atomic<bool> g_swapSinCos{false};
    static std::atomic<double> g_pi{3.141592653589793};
    // custom pi / real pi. Angles passed into trig are multiplied by this and
    // angles returned from inverse trig are too, so "half a turn" == custom pi.
    static std::atomic<double> g_k{1.0};
    constexpr double kRealPi = 3.141592653589793238462643383279502884;

    bool hooksEnabled() { return g_hooksEnabled.load(std::memory_order_relaxed); }
    bool swapSinCos() { return g_swapSinCos.load(std::memory_order_relaxed); }
    double pi() { return g_pi.load(std::memory_order_relaxed); }
    double kd() { return g_k.load(std::memory_order_relaxed); }
    float kf() { return static_cast<float>(kd()); }
    void setPi(double v) { g_pi = v; g_k = v / kRealPi; }

    struct BypassGuard {
        bool previous;
        BypassGuard() : previous(g_bypassOverrides) { g_bypassOverrides = true; }
        ~BypassGuard() { g_bypassOverrides = previous; }
    };

    // Geode routes calls to the original implementation when a hooked symbol
    // is called from a detour on the same thread. The bypass flag also keeps the
    // other function in a sin/cos swap (and any re-entry) from being scaled twice.
    double detourSin(double x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::sin(x);
        BypassGuard guard;
        double a = x * kd();
        return swapSinCos() ? ::cos(a) : ::sin(a);
    }
    double detourCos(double x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::cos(x);
        BypassGuard guard;
        double a = x * kd();
        return swapSinCos() ? ::sin(a) : ::cos(a);
    }
    float detourSinf(float x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::sinf(x);
        BypassGuard guard;
        float a = x * kf();
        return swapSinCos() ? ::cosf(a) : ::sinf(a);
    }
    float detourCosf(float x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::cosf(x);
        BypassGuard guard;
        float a = x * kf();
        return swapSinCos() ? ::sinf(a) : ::cosf(a);
    }
    double detourAcos(double x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::acos(x);
        BypassGuard guard;
        return ::acos(x) * kd();
    }
    float detourAcosf(float x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::acosf(x);
        BypassGuard guard;
        return ::acosf(x) * kf();
    }
    double detourAtan2(double y, double x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::atan2(y, x);
        BypassGuard guard;
        return ::atan2(y, x) * kd();
    }
    float detourAtan2f(float y, float x) {
        if (!hooksEnabled() || g_bypassOverrides) return ::atan2f(y, x);
        BypassGuard guard;
        return ::atan2f(y, x) * kf();
    }

#ifndef GEODE_IS_WINDOWS
    template <class Detour>
    void installMathHook(char const* symbol, Detour detour) {
        auto address = dlsym(RTLD_DEFAULT, symbol);
        if (!address) {
            log::warn("Woopet Math Fucker: a math symbol was not found; skipping that hook");
            return;
        }
        auto result = Mod::get()->hook(
            address,
            detour,
            std::string("WoopetMathFucker::") + symbol,
            tulip::hook::TulipConvention::Cdecl
        );
        if (result.isErr()) {
            log::warn("Woopet Math Fucker: hook installation failed; skipping that hook");
            return;
        }
        log::info("Woopet Math Fucker: installed a math hook");
    }
#endif

    class WoopetMathFuckerPopup final : public Popup {
    protected:
        bool init() {
            if (!Popup::init(290.f, 220.f, "GJ_square01.png")) return false;
            this->setTitle("Woopet Math Fucker");
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
                this, menu_selector(WoopetMathFuckerPopup::onToggleGlobal));
            globalToggle->setPosition({m_size.width - 42.f, m_size.height - 65.f});
            m_buttonMenu->addChild(globalToggle);

            auto* swapLabel = CCLabelBMFont::create("Swap sin and cos", "bigFont.fnt");
            swapLabel->setScale(0.43f); swapLabel->setAnchorPoint({0.f, 0.5f});
            swapLabel->setPosition({24.f, m_size.height - 98.f});
            m_mainLayer->addChild(swapLabel);
            auto* swapToggle = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(swapSinCos() ? "ON" : "OFF", 60, true, "goldFont.fnt", "GJ_button_04.png", 30.f, 0.7f),
                this, menu_selector(WoopetMathFuckerPopup::onToggleSwap));
            swapToggle->setPosition({m_size.width - 42.f, m_size.height - 98.f});
            m_buttonMenu->addChild(swapToggle);

            auto* piLabel = CCLabelBMFont::create("Custom pi value", "bigFont.fnt");
            piLabel->setScale(0.43f); piLabel->setAnchorPoint({0.f, 0.5f});
            piLabel->setPosition({24.f, m_size.height - 131.f});
            m_mainLayer->addChild(piLabel);
            m_piInput = TextInput::create(130.f, "Enter a number", "bigFont.fnt");
            char piText[40];
            std::snprintf(piText, sizeof(piText), "%.15g", pi());
            m_piInput->setString(piText, false);
            m_piInput->setFilter("0123456789.-");
            m_piInput->setPosition({m_size.width - 78.f, m_size.height - 131.f});
            m_mainLayer->addChild(m_piInput);

            auto* save = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png", 0.8f),
                this, menu_selector(WoopetMathFuckerPopup::onSave));
            save->setPosition({m_size.width / 2.f, 35.f});
            m_buttonMenu->addChild(save);

            auto* note = CCLabelBMFont::create("Best effort: not every math operation is hookable.", "bigFont.fnt");
            note->setScale(0.27f); note->setOpacity(190);
            note->setPosition({m_size.width / 2.f, 15.f});
            m_mainLayer->addChild(note);
            return true;
        }
        void reopen() { this->onClose(nullptr); WoopetMathFuckerPopup::create()->show(); }
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
                    FLAlertLayer::create("Woopet Math Fucker", "Enter a finite number greater than 0 and at most 1e12.", "OK")->show();
                    return;
                }
                Mod::get()->setSettingValue("custom-pi", value);
                FLAlertLayer::create("Woopet Math Fucker", "Custom pi saved.", "OK")->show();
            } catch (...) {
                FLAlertLayer::create("Woopet Math Fucker", "That is not a valid number.", "OK")->show();
            }
        }
    public:
        static WoopetMathFuckerPopup* create() {
            auto* ret = new WoopetMathFuckerPopup();
            if (ret->init()) { ret->autorelease(); return ret; }
            delete ret; return nullptr;
        }
    private:
        TextInput* m_piInput = nullptr;
    };
}

$on_mod(Loaded) {
    using namespace woopet::mathfucker;

    g_hooksEnabled = Mod::get()->getSettingValue<bool>("global-math-hooks");
    g_swapSinCos = Mod::get()->getSettingValue<bool>("swap-sin-cos");
    setPi(Mod::get()->getSettingValue<double>("custom-pi"));

    listenForSettingChanges<bool>("global-math-hooks", [](bool v) { g_hooksEnabled = v; });
    listenForSettingChanges<bool>("swap-sin-cos", [](bool v) { g_swapSinCos = v; });
    listenForSettingChanges<double>("custom-pi", [](double v) { if (v > 0.0) setPi(v); });

#ifndef GEODE_IS_WINDOWS
    installMathHook("sin", &detourSin);
    installMathHook("cos", &detourCos);
    installMathHook("sinf", &detourSinf);
    installMathHook("cosf", &detourCosf);
    installMathHook("acos", &detourAcos);
    installMathHook("acosf", &detourAcosf);
    installMathHook("atan2", &detourAtan2);
    installMathHook("atan2f", &detourAtan2f);
#else
    log::warn("Woopet Math Fucker: math hooks are not supported on Windows; only the UI is active");
#endif
}

class $modify(WoopetMathFuckerOptionsLayer, OptionsLayer) {
    void customSetup() {
        OptionsLayer::customSetup();
        auto* button = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Woopet Math Fucker", "goldFont.fnt", "GJ_button_04.png", 0.75f),
            this, menu_selector(WoopetMathFuckerOptionsLayer::onOpenWoopetMathFucker));
        auto* menu = CCMenu::create();
        menu->setID("woopet-math-fucker-menu"); menu->addChild(button);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width / 2.f, 34.f});
        this->addChild(menu);
    }
    void onOpenWoopetMathFucker(CCObject*) { woopet::mathfucker::WoopetMathFuckerPopup::create()->show(); }
};
