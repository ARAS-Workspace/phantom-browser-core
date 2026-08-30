// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/speech/tts_mac.h"

#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/tts_controller.h"

namespace {

constexpr int kNoLength = -1;
constexpr char kNoError[] = "";

constexpr struct {
  const char* name;
  const char* lang;
} kVoices[] = {
    {"System Voice", "en-US"}, {"System Voice", "en-GB"},
    {"System Voice", "tr-TR"}, {"System Voice", "de-DE"},
    {"System Voice", "fr-FR"}, {"System Voice", "es-ES"},
    {"System Voice", "it-IT"}, {"System Voice", "pt-BR"},
    {"System Voice", "ru-RU"}, {"System Voice", "ja-JP"},
    {"System Voice", "zh-CN"}, {"System Voice", "ar-SA"},
};

}  // namespace

const std::vector<content::VoiceData>& TtsPlatformImplMac::Voices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!voices_.empty()) {
    return voices_;
  }
  for (const auto& entry : kVoices) {
    voices_.emplace_back();
    content::VoiceData& data = voices_.back();
    data.native = true;
    data.native_voice_identifier = std::string("system.") + entry.lang;
    data.name = entry.name;
    data.lang = entry.lang;
    data.events.insert(content::TTS_EVENT_START);
    data.events.insert(content::TTS_EVENT_END);
  }
  return voices_;
}

// static
content::TtsPlatformImpl* content::TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplMac::GetInstance();
}

bool TtsPlatformImplMac::PlatformImplSupported() {
  return true;
}

bool TtsPlatformImplMac::PlatformImplInitialized() {
  return true;
}

void TtsPlatformImplMac::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  utterance_ = utterance;
  utterance_id_ = utterance_id;
  paused_ = false;
  std::move(on_speak_finished).Run(true);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TtsPlatformImplMac::CompleteUtterance,
                                base::Unretained(this), utterance_id));
}

void TtsPlatformImplMac::CompleteUtterance(int utterance_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnSpeechEvent(utterance_id, content::TTS_EVENT_START, /*char_index=*/0,
                kNoLength, kNoError);
  OnSpeechEvent(utterance_id, content::TTS_EVENT_END, /*char_index=*/0,
                kNoLength, kNoError);
}

bool TtsPlatformImplMac::StopSpeaking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  paused_ = false;
  return true;
}

void TtsPlatformImplMac::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!paused_) {
    paused_ = true;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_PAUSE, last_char_index_, kNoLength,
        kNoError);
  }
}

void TtsPlatformImplMac::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (paused_) {
    paused_ = false;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_RESUME, last_char_index_, kNoLength,
        kNoError);
  }
}

bool TtsPlatformImplMac::IsSpeaking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

void TtsPlatformImplMac::GetVoices(std::vector<content::VoiceData>* out_voices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  *out_voices = Voices();
}

void TtsPlatformImplMac::RefreshVoices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TtsPlatformImplMac::OnSpeechEvent(int utterance_id,
                                       content::TtsEventType event_type,
                                       int char_index,
                                       int char_length,
                                       const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't send events from an utterance that's already completed.
  if (utterance_id != utterance_id_) {
    return;
  }

  if (event_type == content::TTS_EVENT_END) {
    char_index = utterance_.size();
  }

  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, event_type, char_index, char_length, error_message);
  last_char_index_ = char_index;
}

TtsPlatformImplMac::TtsPlatformImplMac() = default;

TtsPlatformImplMac::~TtsPlatformImplMac() = default;

// static
TtsPlatformImplMac* TtsPlatformImplMac::GetInstance() {
  static base::NoDestructor<TtsPlatformImplMac> tts_platform;
  return tts_platform.get();
}
