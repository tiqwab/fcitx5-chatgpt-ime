#ifndef _FCITX5_CHATGPT_ENGINE_H_
#define _FCITX5_CHATGPT_ENGINE_H_

#include <fcitx/inputmethodengine.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx-utils/inputbuffer.h>
#include <fcitx/instance.h>

class ChatGPTEngine;

class ChatGPTState : public fcitx::InputContextProperty {
public:
    ChatGPTState(ChatGPTEngine *engine, fcitx::InputContext *ic): engine_(engine), ic_(ic), is_converting_(false) {}

    void keyEvent(fcitx::KeyEvent & keyEvent);
    void reset();

private:
    ChatGPTEngine *engine_;
    fcitx::InputContext *ic_;
    // fcitx::InputBuffer buffer_{{fcitx::InputBufferOption::AsciiOnly, fcitx::InputBufferOption::FixedCursor}};
    fcitx::InputBuffer buffer_{{fcitx::InputBufferOption::FixedCursor}};
    bool is_converting_;

    void keyEventWhenNotConverting(fcitx::KeyEvent &keyEvent);
    void keyEventWhenConverting(fcitx::KeyEvent &keyEvent);
};

class ChatGPTEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit ChatGPTEngine(fcitx::Instance *instance);

    void keyEvent(const fcitx::InputMethodEntry & entry, fcitx::KeyEvent & keyEvent) override;
    void reset(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) override;

private:
    fcitx::Instance *instance_;
    fcitx::FactoryFor<ChatGPTState> factory_;
};

class ChatGPTEngineFactory : public fcitx::AddonFactory {
    fcitx::AddonInstance * create(fcitx::AddonManager * manager) override {
        return new ChatGPTEngine(manager->instance());
    }
};

#endif // _FCITX5_CHATGPT_ENGINE_H_
