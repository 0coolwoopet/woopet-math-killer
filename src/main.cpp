#include <Geode/Geode.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>
#include <atomic>
#include <string>

#ifndef GEODE_IS_WINDOWS
    #include <dlfcn.h>
#endif

using namespace geode::prelude;

namespace woopet::mathfucker {
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

    // Own sin/cos (fdlibm kernels). The sin/cos/tan/sincos detours must NOT call
    // other hooked libm functions: calling a different hooked symbol from inside
    // a detour is what crashed the sin/cos swap. These never touch hooked symbols.
    namespace impl {
        constexpr double pio2_1  = 1.57079632673412561417e+00;
        constexpr double pio2_1t = 6.07710050650619224932e-11;
        constexpr double invpio2 = 6.36619772367581382433e-01;

        inline double kernelSin(double x) {
            double z = x * x;
            double r = 8.33333333332248946124e-03 + z * (-1.98412698298579493134e-04 +
                       z * (2.75573137070700676789e-06 + z * (-2.50507602534068634195e-08 +
                       z * 1.58969099521155010221e-10)));
            return x + x * z * (-1.66666666666666324348e-01 + z * r);
        }
        inline double kernelCos(double x) {
            double z = x * x;
            double r = 4.16666666666666019037e-02 + z * (-1.38888888888741095749e-03 +
                       z * (2.48015872894767294178e-05 + z * (-2.75573143513906633035e-07 +
                       z * (2.08757232129817482790e-09 + z * -1.13596475577881948265e-11))));
            return 1.0 - 0.5 * z + z * z * r;
        }
        inline void sincos(double x, double& s, double& c) {
            if (!std::isfinite(x)) {
                s = c = std::numeric_limits<double>::quiet_NaN();
                return;
            }
            if (std::fabs(x) > 1.0e5) x = std::fmod(x, 2.0 * kRealPi);
            double nf = std::nearbyint(x * invpio2);
            long long n = static_cast<long long>(nf);
            double r = (x - nf * pio2_1) - nf * pio2_1t;
            double sr = kernelSin(r), cr = kernelCos(r);
            switch (n & 3) {
                case 0: s = sr;  c = cr;  break;
                case 1: s = cr;  c = -sr; break;
                case 2: s = -sr; c = -cr; break;
                default: s = -cr; c = sr; break;
            }
        }
        // Scaled by custom pi, optionally swapped. Returns {sin, cos}.
        inline void scaled(double x, double k, bool swap, double& s, double& c) {
            sincos(x * k, s, c);
            if (swap) std::swap(s, c);
        }
    }

    double detourSin(double x) {
        if (!hooksEnabled()) return ::sin(x);
        double s, c; impl::scaled(x, kd(), swapSinCos(), s, c);
        return s;
    }
    double detourCos(double x) {
        if (!hooksEnabled()) return ::cos(x);
        double s, c; impl::scaled(x, kd(), swapSinCos(), s, c);
        return c;
    }
    float detourSinf(float x) {
        if (!hooksEnabled()) return ::sinf(x);
        double s, c; impl::scaled(static_cast<double>(x), kd(), swapSinCos(), s, c);
        return static_cast<float>(s);
    }
    float detourCosf(float x) {
        if (!hooksEnabled()) return ::cosf(x);
        double s, c; impl::scaled(static_cast<double>(x), kd(), swapSinCos(), s, c);
        return static_cast<float>(c);
    }
    // sin and cos of the same angle are often merged by the compiler into one
    // sincos call (hitbox / rotation math), so hook that too.
    void detourSincos(double x, double* s, double* c) {
        double sv, cv;
        impl::scaled(x, hooksEnabled() ? kd() : 1.0, hooksEnabled() && swapSinCos(), sv, cv);
        *s = sv; *c = cv;
    }
    void detourSincosf(float x, float* s, float* c) {
        double sv, cv;
        impl::scaled(static_cast<double>(x), hooksEnabled() ? kd() : 1.0, hooksEnabled() && swapSinCos(), sv, cv);
        *s = static_cast<float>(sv); *c = static_cast<float>(cv);
    }
    double detourTan(double x) {
        if (!hooksEnabled()) return ::tan(x);
        double s, c; impl::sincos(x * kd(), s, c);
        return s / c;
    }
    float detourTanf(float x) {
        if (!hooksEnabled()) return ::tanf(x);
        double s, c; impl::sincos(static_cast<double>(x) * kd(), s, c);
        return static_cast<float>(s / c);
    }

    // Inverse functions only call their own (hooked) symbol, which is the
    // same-function re-entry Geode supports.
    double detourAcos(double x) {
        return hooksEnabled() ? ::acos(x) * kd() : ::acos(x);
    }
    float detourAcosf(float x) {
        return hooksEnabled() ? ::acosf(x) * kf() : ::acosf(x);
    }
    double detourAsin(double x) {
        return hooksEnabled() ? ::asin(x) * kd() : ::asin(x);
    }
    float detourAsinf(float x) {
        return hooksEnabled() ? ::asinf(x) * kf() : ::asinf(x);
    }
    double detourAtan(double x) {
        return hooksEnabled() ? ::atan(x) * kd() : ::atan(x);
    }
    float detourAtanf(float x) {
        return hooksEnabled() ? ::atanf(x) * kf() : ::atanf(x);
    }
    double detourAtan2(double y, double x) {
        return hooksEnabled() ? ::atan2(y, x) * kd() : ::atan2(y, x);
    }
    float detourAtan2f(float y, float x) {
        return hooksEnabled() ? ::atan2f(y, x) * kf() : ::atan2f(y, x);
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
            save->setPosition({m_size.width / 2.f - 50.f, 35.f});
            m_buttonMenu->addChild(save);

            auto* reset = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Reset", "goldFont.fnt", "GJ_button_06.png", 0.8f),
                this, menu_selector(WoopetMathFuckerPopup::onResetPi));
            reset->setPosition({m_size.width / 2.f + 50.f, 35.f});
            m_buttonMenu->addChild(reset);

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
        void onResetPi(CCObject*) {
            Mod::get()->setSettingValue("custom-pi", kRealPi);
            if (m_piInput) {
                char piText[40];
                std::snprintf(piText, sizeof(piText), "%.15g", kRealPi);
                m_piInput->setString(piText, false);
            }
            FLAlertLayer::create("Woopet Math Fucker", "Pi reset to default.", "OK")->show();
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
    installMathHook("sincos", &detourSincos);
    installMathHook("sincosf", &detourSincosf);
    installMathHook("tan", &detourTan);
    installMathHook("tanf", &detourTanf);
    installMathHook("asin", &detourAsin);
    installMathHook("asinf", &detourAsinf);
    installMathHook("atan", &detourAtan);
    installMathHook("atanf", &detourAtanf);
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
