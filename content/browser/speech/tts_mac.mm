// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/speech/tts_mac.h"

#import <AVFAudio/AVFAudio.h>
#import <AppKit/AppKit.h>
#include <objc/runtime.h>

#include <algorithm>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
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

int GetUtteranceId(AVSpeechUtterance* utterance) {
  NSNumber* identifier = base::apple::ObjCCast<NSNumber>(
      objc_getAssociatedObject(utterance, @selector(identifier)));
  if (identifier) {
    return identifier.intValue;
  }
  return TtsPlatformImplMac::kInvalidUtteranceId;
}

}  // namespace

AVSpeechSynthesisVoice*
TtsPlatformImplMacBackgroundWorker::GetSystemDefaultVoice() {
  return nil;
}

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

TtsPlatformImplMac::~TtsPlatformImplMac() {
  if (application_active_observer_) {
    [NSNotificationCenter.defaultCenter
        removeObserver:application_active_observer_];
  }
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
  OnSpeechEvent(utterance_id, content::TTS_EVENT_START, /*char_index=*/0,
                kNoLength, kNoError);
  OnSpeechEvent(utterance_id, content::TTS_EVENT_END, /*char_index=*/0,
                kNoLength, kNoError);
}

void TtsPlatformImplMac::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished,
    const std::string& parsed_utterance) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(on_speak_finished).Run(false);
}

bool TtsPlatformImplMac::StopSpeaking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [speech_synthesizer_ stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
  paused_ = false;
  return true;
}

void TtsPlatformImplMac::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!paused_) {
    [speech_synthesizer_ pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    paused_ = true;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_PAUSE, last_char_index_, kNoLength,
        kNoError);
  }
}

void TtsPlatformImplMac::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (paused_) {
    [speech_synthesizer_ continueSpeaking];
    paused_ = false;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_RESUME, last_char_index_, kNoLength,
        kNoError);
  }
}

bool TtsPlatformImplMac::IsSpeaking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return speech_synthesizer_.speaking;
}

void TtsPlatformImplMac::GetVoices(std::vector<content::VoiceData>* outVoices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  *outVoices = Voices();
}

void TtsPlatformImplMac::RefreshVoices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  received_voices_request_ = true;
  UpdateSystemDefaultVoice();
}

void TtsPlatformImplMac::UpdateSystemDefaultVoice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TtsPlatformImplMac::OnGotDefaultVoice(
    AVSpeechSynthesisVoice* default_voice) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_updating_default_voice_ = false;
  bool default_voice_changed =
      (default_voice_ != default_voice &&
       (!default_voice_ || !default_voice ||
        ![default_voice_.identifier isEqualToString:default_voice.identifier]));

  default_voice_ = default_voice;
  if (default_voice_changed) {
    voices_.clear();
    Voices();
    content::TtsController::GetInstance()->VoicesChanged();
  }

  if (needs_reupdate_default_voice_) {
    needs_reupdate_default_voice_ = false;
    UpdateSystemDefaultVoice();
  }
}

void TtsPlatformImplMac::OnApplicationWillBecomeActive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!received_voices_request_) {
    return;
  }
  UpdateSystemDefaultVoice();
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

// static
TtsPlatformImplMac* TtsPlatformImplMac::GetInstance() {
  static base::NoDestructor<TtsPlatformImplMac> tts_platform;
  return tts_platform.get();
}

// static
base::SequenceBound<TtsPlatformImplMacBackgroundWorker>&
TtsPlatformImplMac::GetBackgroundWorker() {
  static base::NoDestructor<
      base::SequenceBound<TtsPlatformImplMacBackgroundWorker>>
      worker(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}));
  return *worker;
}

@implementation ChromeTtsDelegate {
  raw_ptr<TtsPlatformImplMac> _ttsImplMac;  // weak.
}

- (id)initWithPlatformImplMac:(TtsPlatformImplMac*)ttsImplMac {
  if ((self = [super init])) {
    _ttsImplMac = ttsImplMac;
  }
  return self;
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    didStartSpeechUtterance:(AVSpeechUtterance*)utterance {
  _ttsImplMac->OnSpeechEvent(GetUtteranceId(utterance),
                             content::TTS_EVENT_START, /*char_index=*/0,
                             kNoLength, kNoError);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    didFinishSpeechUtterance:(AVSpeechUtterance*)utterance {
  _ttsImplMac->OnSpeechEvent(GetUtteranceId(utterance), content::TTS_EVENT_END,
                             /*char_index=*/0, kNoLength, kNoError);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    willSpeakRangeOfSpeechString:(NSRange)characterRange
                       utterance:(AVSpeechUtterance*)utterance {
  // Ignore bogus ranges. The Mac speech synthesizer is a bit buggy and
  // occasionally returns a number way out of range.
  if (characterRange.location > utterance.speechString.length ||
      characterRange.length == 0) {
    return;
  }
  _ttsImplMac->OnSpeechEvent(GetUtteranceId(utterance), content::TTS_EVENT_WORD,
                             characterRange.location, characterRange.length,
                             kNoError);
}

@end
