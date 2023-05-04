#include "engine.h"
#include "hiraganatable.h"
#include "imeclient.h"
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonfactory.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/userinterfacemanager.h>

const char *CHATGPT_IME_SOCKET_FILE_PATH = "/tmp/chatgpt-ime.sock";

class ChatGPTCandidateWord : public fcitx::CandidateWord {
public:
    explicit ChatGPTCandidateWord(const std::string &display): fcitx::CandidateWord(fcitx::Text(display)) {}

    void select(fcitx::InputContext *ic) const override {
        // do nothing
    }
};

void ChatGPTState::keyEvent(fcitx::KeyEvent &keyEvent) {
    if (is_converting_) {
        keyEventWhenConverting(keyEvent);
    } else {
        keyEventWhenNotConverting(keyEvent);
    }
}

void ChatGPTState::keyEventWhenNotConverting(fcitx::KeyEvent &keyEvent) {
    auto ic = keyEvent.inputContext();

    if (keyEvent.key().check(FcitxKey_BackSpace)) {
        if (!buffer_.empty()) {
            buffer_.backspace();
            keyEvent.filterAndAccept();
        }
    } else if (keyEvent.key().check(FcitxKey_Return)) {
        if (!buffer_.empty()) {
            ic->commitString(buffer_.userInput());
            reset();
            ic->inputPanel().reset();
            keyEvent.filterAndAccept();
        }
    } else if (keyEvent.key().check(FcitxKey_space)) {
        if (!buffer_.empty()) {
            IMEClient ime_client(CHATGPT_IME_SOCKET_FILE_PATH);
            auto response = ime_client.send_message(buffer_.userInput());
            if (response.is_success) {
                auto candidates = std::make_unique<fcitx::CommonCandidateList>();
                candidates->setLayoutHint(fcitx::CandidateLayoutHint::Vertical);
                for (const auto& candidate : response.candidates) {
                    candidates->append<ChatGPTCandidateWord>(candidate);
                }
                // the first index is selected in the initial state (for simplicity)
                candidates->toCursorMovable()->nextCandidate();
                ic->inputPanel().setCandidateList(std::move(candidates));

                is_converting_ = true;
                keyEvent.filterAndAccept();
            } else {
                FCITX_ERROR() << "failure response from IME: " + response.error_message;
            }
        }
    } else if (keyEvent.key().isSimple()) {
        buffer_.type(keyEvent.key().sym());
        auto s = buffer_.userInput();
        auto t = convert_to_hiragana(s);
        if (!t.empty()) {
            buffer_.clear();
            buffer_.type(t);
        }
        keyEvent.filterAndAccept();
    } else {
        // do nothing
    }

    fcitx::Text preedit;
    if (is_converting_) {
        preedit.append(buffer_.userInput(), fcitx::TextFormatFlag::HighLight);
    } else {
        preedit.append(buffer_.userInput(), fcitx::TextFormatFlag::Underline);
    }

    if (ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
        preedit.setCursor(preedit.textLength());
        ic->inputPanel().setClientPreedit(preedit);
    } else {
        ic->inputPanel().setPreedit(preedit);
    }
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void ChatGPTState::keyEventWhenConverting(fcitx::KeyEvent &keyEvent) {
    auto ic = keyEvent.inputContext();
    auto candidates = ic->inputPanel().candidateList();

    if (keyEvent.key().check(FcitxKey_BackSpace)) {
        ic->inputPanel().reset();
        is_converting_ = false;
        keyEvent.filterAndAccept();
    } else if (keyEvent.key().check(FcitxKey_Up)) {
        candidates->toCursorMovable()->prevCandidate();
        keyEvent.filterAndAccept();
    } else if (keyEvent.key().check(FcitxKey_space) || keyEvent.key().check(FcitxKey_Down)) {
        candidates->toCursorMovable()->nextCandidate();
        keyEvent.filterAndAccept();
    } else if (keyEvent.key().check(FcitxKey_Return)) {
        ic->commitString(candidates->candidate(candidates->cursorIndex()).text().toStringForCommit());
        reset();
        ic->inputPanel().reset();
        is_converting_ = false;
        keyEvent.filterAndAccept();
    } else if (keyEvent.key().isSimple()) {
        // same as Return, but type the new key
        ic->commitString(candidates->candidate(candidates->cursorIndex()).text().toStringForCommit());
        reset();
        ic->inputPanel().reset();

        buffer_.type(keyEvent.key().sym());
        auto s = buffer_.userInput();
        auto t = convert_to_hiragana(s);
        if (!t.empty()) {
            buffer_.clear();
            buffer_.type(t);
        }

        is_converting_ = false;
        keyEvent.filterAndAccept();
    } else {
        // do nothing
    }

    fcitx::Text preedit;
    if (is_converting_) {
        preedit.append(buffer_.userInput(), fcitx::TextFormatFlag::HighLight);
    } else {
        preedit.append(buffer_.userInput(), fcitx::TextFormatFlag::Underline);
    }

    if (ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
        preedit.setCursor(preedit.textLength());
        ic->inputPanel().setClientPreedit(preedit);
    } else {
        ic->inputPanel().setPreedit(preedit);
    }
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void ChatGPTState::reset() {
    buffer_.clear();
    is_converting_ = false;
}

ChatGPTEngine::ChatGPTEngine(fcitx::Instance *instance) : instance_(instance), factory_([this](fcitx::InputContext &ic) {
    FCITX_INFO() << "create state";
    return new ChatGPTState(this, &ic);
}) {
    instance->inputContextManager().registerProperty("chatgptState", &factory_);
}

void ChatGPTEngine::keyEvent(const fcitx::InputMethodEntry& entry, fcitx::KeyEvent& keyEvent) {
    FCITX_UNUSED(entry);
    FCITX_INFO() << keyEvent.key() << " isRelease=" << keyEvent.isRelease();

    // キーのプッシュアップ時や修飾キーとの入力は無視する
    if (keyEvent.isRelease() || keyEvent.key().states()) {
        return;
    }

    auto ic = keyEvent.inputContext();
    auto *state = ic->propertyFor(&factory_);

    state->keyEvent(keyEvent);
}

void ChatGPTEngine::reset(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) {
    FCITX_UNUSED(entry);
    FCITX_INFO() << "reset";

    auto ic = event.inputContext();

    auto *state = ic->propertyFor(&factory_);
    state->reset();

    ic->inputPanel().reset();
}

FCITX_ADDON_FACTORY(ChatGPTEngineFactory);
