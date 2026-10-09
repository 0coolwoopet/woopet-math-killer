#include <Geode/Geode.hpp>
#include <Geode/modify/OptionsLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

#ifndef GEODE_IS_WINDOWS
#include <dlfcn.h>
#endif

using namespace geode::prelude;

constexpr double REAL_PI = 3.14159265358979323846;

static std::atomic<bool> hooksOn{true};
static std::atomic<bool> swapOn{false};
static std::atomic<double> piValue{REAL_PI};
static std::atomic<double> piScale{1.0};

static void setPi(double v) {
    piValue = v;
    piScale = v / REAL_PI;
}

static double scaleD() { return piScale.load(std::memory_order_relaxed); }

template <class T>
static T scaleFor() {
    return hooksOn ? static_cast<T>(scaleD()) : T(1);
}

static double sinPoly(double x) {
    double z = x * x;
    double r = 8.33333333332248946124e-03 + z * (-1.98412698298579493134e-04 +
               z * (2.75573137070700676789e-06 + z * (-2.50507602534068634195e-08 +
               z * 1.58969099521155010221e-10)));
    return x + x * z * (-1.66666666666666324348e-01 + z * r);
}

static double cosPoly(double x) {
    double z = x * x;
    double r = 4.16666666666666019037e-02 + z * (-1.38888888888741095749e-03 +
               z * (2.48015872894767294178e-05 + z * (-2.75573143513906633035e-07 +
               z * (2.08757232129817482790e-09 + z * -1.13596475577881948265e-11))));
    return 1.0 - 0.5 * z + z * z * r;
}

static void sinCos(double x, double& s, double& c) {
    if (!std::isfinite(x)) {
        s = c = std::numeric_limits<double>::quiet_NaN();
        return;
    }
    if (std::fabs(x) > 1e5) x = std::fmod(x, 2.0 * REAL_PI);

    double n = std::nearbyint(x * 6.36619772367581382433e-01);
    double r = (x - n * 1.57079632673412561417e+00) - n * 6.07710050650619224932e-11;
    double sr = sinPoly(r);
    double cr = cosPoly(r);

    switch (static_cast<long long>(n) & 3) {
        case 0: s = sr;  c = cr;  break;
        case 1: s = cr;  c = -sr; break;
        case 2: s = -sr; c = -cr; break;
        default: s = -cr; c = sr; break;
    }
}

static void trig(double x, double& s, double& c) {
    if (!hooksOn) return sinCos(x, s, c);
    sinCos(x * scaleD(), s, c);
    if (swapOn) std::swap(s, c);
}

template <class T>
static T hookSin(T x) {
    if (!hooksOn) return std::sin(x);
    double s, c;
    trig(x, s, c);
    return static_cast<T>(s);
}

template <class T>
static T hookCos(T x) {
    if (!hooksOn) return std::cos(x);
    double s, c;
    trig(x, s, c);
    return static_cast<T>(c);
}

template <class T>
static void hookSincos(T x, T* s, T* c) {
    double sv, cv;
    trig(x, sv, cv);
    *s = static_cast<T>(sv);
    *c = static_cast<T>(cv);
}

template <class T>
static T hookTan(T x) {
    if (!hooksOn) return std::tan(x);
    double s, c;
    sinCos(x * scaleD(), s, c);
    return static_cast<T>(s / c);
}

template <class T>
static T hookAcos(T x) { return std::acos(x) * scaleFor<T>(); }

template <class T>
static T hookAsin(T x) { return std::asin(x) * scaleFor<T>(); }

template <class T>
static T hookAtan(T x) { return std::atan(x) * scaleFor<T>(); }

template <class T>
static T hookAtan2(T y, T x) { return std::atan2(y, x) * scaleFor<T>(); }

static void hookMath(char const* name, auto detour) {
#ifndef GEODE_IS_WINDOWS
    void* address = dlsym(RTLD_DEFAULT, name);
    if (!address) {
        log::debug("{} not found, skipping", name);
        return;
    }
    auto res = Mod::get()->hook(
        address, detour, std::string("math::") + name,
        tulip::hook::TulipConvention::Cdecl
    );
    if (res.isErr()) log::warn("failed to hook {}", name);
#endif
}

static std::string piText(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.15g", v);
    return buf;
}

class MathPopup : public Popup {
protected:
    TextInput* m_input = nullptr;
    ButtonSprite* m_hooksLabel = nullptr;
    ButtonSprite* m_swapLabel = nullptr;

    void addLabel(char const* text, float y) {
        auto label = CCLabelBMFont::create(text, "bigFont.fnt");
        label->setScale(0.43f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({24.f, m_size.height - y});
        m_mainLayer->addChild(label);
    }

    ButtonSprite* addToggle(char const* key, SEL_MenuHandler cb, float y) {
        auto sprite = ButtonSprite::create(
            Mod::get()->getSettingValue<bool>(key) ? "ON" : "OFF",
            60, true, "goldFont.fnt", "GJ_button_04.png", 30.f, 0.7f
        );
        auto btn = CCMenuItemSpriteExtra::create(sprite, this, cb);
        btn->setPosition({m_size.width - 42.f, m_size.height - y});
        m_buttonMenu->addChild(btn);
        return sprite;
    }

    void addButton(char const* text, char const* texture, SEL_MenuHandler cb, float x) {
        auto sprite = ButtonSprite::create(text, "goldFont.fnt", texture, 0.8f);
        auto btn = CCMenuItemSpriteExtra::create(sprite, this, cb);
        btn->setPosition({x, 35.f});
        m_buttonMenu->addChild(btn);
    }

    bool init() {
        if (!Popup::init(290.f, 220.f, "GJ_square01.png")) return false;
        setTitle("Woopet Math Fucker");

        addLabel("Global math hooks", 65.f);
        m_hooksLabel = addToggle("global-math-hooks", menu_selector(MathPopup::onHooks), 65.f);

        addLabel("Swap sin and cos", 98.f);
        m_swapLabel = addToggle("swap-sin-cos", menu_selector(MathPopup::onSwap), 98.f);

        addLabel("Custom pi value", 131.f);
        m_input = TextInput::create(130.f, "Enter a number", "bigFont.fnt");
        m_input->setFilter("0123456789.-");
        m_input->setString(piText(piValue), false);
        m_input->setPosition({m_size.width - 78.f, m_size.height - 131.f});
        m_mainLayer->addChild(m_input);

        addButton("Save", "GJ_button_01.png", menu_selector(MathPopup::onSave), m_size.width / 2.f - 50.f);
        addButton("Reset", "GJ_button_06.png", menu_selector(MathPopup::onReset), m_size.width / 2.f + 50.f);

        auto note = CCLabelBMFont::create("Best effort: not every math operation is hookable.", "bigFont.fnt");
        note->setScale(0.27f);
        note->setOpacity(190);
        note->setPosition({m_size.width / 2.f, 15.f});
        m_mainLayer->addChild(note);
        return true;
    }

    void toggle(char const* key, ButtonSprite* label) {
        bool value = !Mod::get()->getSettingValue<bool>(key);
        Mod::get()->setSettingValue(key, value);
        label->setString(value ? "ON" : "OFF");
    }

    void onHooks(CCObject*) { toggle("global-math-hooks", m_hooksLabel); }
    void onSwap(CCObject*) { toggle("swap-sin-cos", m_swapLabel); }

    void onSave(CCObject*) {
        std::string text = m_input->getString();
        char* end = nullptr;
        double value = std::strtod(text.c_str(), &end);
        if (text.empty() || *end != '\0' || !(value > 0.0) || value > 1e12) {
            FLAlertLayer::create("Invalid pi", "Enter a number above 0 and up to 1e12.", "OK")->show();
            return;
        }
        Mod::get()->setSettingValue("custom-pi", value);
    }

    void onReset(CCObject*) {
        Mod::get()->setSettingValue("custom-pi", REAL_PI);
        m_input->setString(piText(REAL_PI), false);
    }

public:
    static MathPopup* create() {
        auto ret = new MathPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(MathOptionsLayer, OptionsLayer) {
    void customSetup() {
        OptionsLayer::customSetup();

        auto sprite = ButtonSprite::create("Woopet Math Fucker", "goldFont.fnt", "GJ_button_04.png", 0.75f);
        auto btn = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(MathOptionsLayer::onMath));
        auto menu = CCMenu::create();
        menu->setID("math-menu");
        menu->addChild(btn);
        menu->setPosition({CCDirector::get()->getWinSize().width / 2.f, 34.f});
        addChild(menu);
    }

    void onMath(CCObject*) {
        MathPopup::create()->show();
    }
};

$on_mod(Loaded) {
    auto mod = Mod::get();
    hooksOn = mod->getSettingValue<bool>("global-math-hooks");
    swapOn = mod->getSettingValue<bool>("swap-sin-cos");
    setPi(mod->getSettingValue<double>("custom-pi"));

    listenForSettingChanges<bool>("global-math-hooks", [](bool v) { hooksOn = v; });
    listenForSettingChanges<bool>("swap-sin-cos", [](bool v) { swapOn = v; });
    listenForSettingChanges<double>("custom-pi", [](double v) {
        if (v > 0.0) setPi(v);
    });

    hookMath("sin", &hookSin<double>);
    hookMath("sinf", &hookSin<float>);
    hookMath("cos", &hookCos<double>);
    hookMath("cosf", &hookCos<float>);
    hookMath("sincos", &hookSincos<double>);
    hookMath("sincosf", &hookSincos<float>);
    hookMath("tan", &hookTan<double>);
    hookMath("tanf", &hookTan<float>);
    hookMath("acos", &hookAcos<double>);
    hookMath("acosf", &hookAcos<float>);
    hookMath("asin", &hookAsin<double>);
    hookMath("asinf", &hookAsin<float>);
    hookMath("atan", &hookAtan<double>);
    hookMath("atanf", &hookAtan<float>);
    hookMath("atan2", &hookAtan2<double>);
    hookMath("atan2f", &hookAtan2<float>);
}
