/**
 * @file MQTTRx.cpp
 * @brief MQTT receive-path parsing and acknowledgements for MQTTModule.
 */

#include "MQTTModule.h"

#include "Core/BufferUsageTracker.h"
#include "Core/MqttTopics.h"
#include "Modules/Network/MQTTModule/MQTTRuntime.h"

#include <ArduinoJson.h>
#include <string.h>

#define LOG_MODULE_ID ((LogModuleId)LogModuleIdValue::MQTTModule)
#include "Core/ModuleLog.h"

namespace {
bool mqttCommandDeniedByPolicy_(const char* cmd)
{
    if (!cmd || cmd[0] == '\0') return false;
    if (strncmp(cmd, "fw.update.", 10U) == 0 &&
        strcmp(cmd, "fw.update.status") != 0) {
        return true;
    }
    return strcmp(cmd, "config.import") == 0 ||
           strcmp(cmd, "config.restore") == 0 ||
           strcmp(cmd, "cfg.import") == 0 ||
           strcmp(cmd, "cfg.restore") == 0;
}

bool mqttCommandNameValid_(const char* cmd, size_t capacity)
{
    if (!cmd || cmd[0] == '\0' || capacity < 2U) return false;
    const size_t len = strlen(cmd);
    if (len >= capacity) return false;
    for (size_t i = 0U; i < len; ++i) {
        const char c = cmd[i];
        const bool allowed =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '-';
        if (!allowed) return false;
    }
    return true;
}
}  // namespace

bool MQTTModule::admitInbound_(const char* topic, uint32_t nowMs, bool& reportRejection)
{
    reportRejection = false;

    if (inboundBlockedUntilMs_ != 0U &&
        (int32_t)(inboundBlockedUntilMs_ - nowMs) > 0) {
        ++inboundRateLimitedCount_;
        if (inboundLastRejectReportMs_ == 0U ||
            (uint32_t)(nowMs - inboundLastRejectReportMs_) >=
                Limits::Mqtt::Security::InboundRejectReportMs) {
            inboundLastRejectReportMs_ = nowMs;
            reportRejection = true;
            LOGW("mqtt inbound rate limited topic=%s remaining_ms=%lu rejected=%lu",
                 topic ? topic : "<null>",
                 (unsigned long)(inboundBlockedUntilMs_ - nowMs),
                 (unsigned long)inboundRateLimitedCount_);
        }
        return false;
    }

    if (inboundBlockedUntilMs_ != 0U) {
        inboundBlockedUntilMs_ = 0U;
        inboundWindowStartMs_ = nowMs;
        inboundAcceptedInWindow_ = 0U;
    }

    if (inboundWindowStartMs_ == 0U ||
        (uint32_t)(nowMs - inboundWindowStartMs_) >=
            Limits::Mqtt::Security::InboundWindowMs) {
        inboundWindowStartMs_ = nowMs;
        inboundAcceptedInWindow_ = 0U;
    }

    if (inboundAcceptedInWindow_ >=
        Limits::Mqtt::Security::InboundMaxPerWindow) {
        inboundBlockedUntilMs_ =
            nowMs + Limits::Mqtt::Security::InboundBlockMs;
        inboundLastRejectReportMs_ = nowMs;
        ++inboundRateLimitedCount_;
        reportRejection = true;
        LOGW("mqtt inbound burst blocked topic=%s accepted=%u window_ms=%lu block_ms=%lu",
             topic ? topic : "<null>",
             (unsigned)inboundAcceptedInWindow_,
             (unsigned long)Limits::Mqtt::Security::InboundWindowMs,
             (unsigned long)Limits::Mqtt::Security::InboundBlockMs);
        return false;
    }

    ++inboundAcceptedInWindow_;
    return true;
}

void MQTTModule::processRx_(const RxMsg& msg)
{
    const char* inboundKind =
        strcmp(msg.topic, topicCmd_) == 0
            ? "cmd"
            : (strcmp(msg.topic, topicCfgSet_) == 0 ? "cfg/set" : "handler");
    bool reportRateLimit = false;
    if (!admitInbound_(inboundKind, millis(), reportRateLimit)) {
        countRxDrop_();
        if (reportRateLimit) {
            const char* ackSuffix =
                strcmp(msg.topic, topicCfgSet_) == 0
                    ? MqttTopics::SuffixCfgAck
                    : MqttTopics::SuffixAck;
            publishRxError_(ackSuffix, ErrorCode::RateLimited, "mqtt.rx", false);
        }
        return;
    }

    if (strcmp(msg.topic, topicCmd_) == 0) {
        processRxCmd_(msg);
        return;
    }

    if (strcmp(msg.topic, topicCfgSet_) == 0) {
        processRxCfgSet_(msg);
        return;
    }

    const MqttInboundHandler* handlers[MaxInboundHandlers]{};
    uint8_t handlerCount = 0;
    portENTER_CRITICAL(&inboundMux_);
    handlerCount = inboundHandlerCount_;
    if (handlerCount > MaxInboundHandlers) handlerCount = MaxInboundHandlers;
    for (uint8_t i = 0; i < handlerCount; ++i) {
        handlers[i] = inboundHandlers_[i];
    }
    portEXIT_CRITICAL(&inboundMux_);

    for (uint8_t i = 0; i < handlerCount; ++i) {
        const MqttInboundHandler* h = handlers[i];
        if (!h || !h->topicSuffix || !h->onMessage) continue;
        char topic[Limits::Mqtt::Buffers::Topic] = {0};
        formatTopic(topic, sizeof(topic), h->topicSuffix);
        if (strcmp(msg.topic, topic) != 0) continue;

        const MqttInboundMessage inbound{msg.topic, msg.payload};
        LOGI("mqtt inbound handler accepted");
        h->onMessage(h->ctx, inbound);
        return;
    }

    publishRxError_(MqttTopics::SuffixAck, ErrorCode::UnknownTopic, "rx", false);
}

void MQTTModule::processRxCmd_(const RxMsg& msg)
{
    if (!scratch_) return;

    static constexpr size_t CMD_DOC_CAPACITY = Limits::JsonCmdBuf;
    static StaticJsonDocument<CMD_DOC_CAPACITY> doc;

    doc.clear();
    DeserializationError err = deserializeJson(doc, msg.payload);
    if (err || !doc.is<JsonObjectConst>()) {
        BufferUsageTracker::note(TrackedBufferId::MqttCmdDoc,
                                 doc.memoryUsage(),
                                 sizeof(doc),
                                 "cmd",
                                 nullptr);
        publishRxError_(MqttTopics::SuffixAck, ErrorCode::BadCmdJson, "cmd", true);
        return;
    }

    JsonObjectConst root = doc.as<JsonObjectConst>();
    JsonVariantConst cmdVar = root["cmd"];
    if (!cmdVar.is<const char*>()) {
        publishRxError_(MqttTopics::SuffixAck, ErrorCode::MissingCmd, "cmd", true);
        return;
    }

    const char* cmdVal = cmdVar.as<const char*>();
    if (!cmdVal || cmdVal[0] == '\0') {
        publishRxError_(MqttTopics::SuffixAck, ErrorCode::MissingCmd, "cmd", true);
        return;
    }
    BufferUsageTracker::note(TrackedBufferId::MqttCmdDoc,
                             doc.memoryUsage(),
                             sizeof(doc),
                             cmdVal,
                             nullptr);

    if (!cmdSvc_ || !cmdSvc_->execute) {
        publishRxError_(MqttTopics::SuffixAck, ErrorCode::CmdServiceUnavailable, "cmd", false);
        return;
    }

    char cmd[Limits::Mqtt::Buffers::CmdName] = {0};
    if (!mqttCommandNameValid_(cmdVal, sizeof(cmd))) {
        publishRxError_(MqttTopics::SuffixAck, ErrorCode::BadCmdJson, "cmd.name", true);
        return;
    }
    const size_t cmdLen = strlen(cmdVal);
    memcpy(cmd, cmdVal, cmdLen);
    cmd[cmdLen] = '\0';

    if (mqttCommandDeniedByPolicy_(cmd)) {
        LOGW("mqtt command denied by policy cmd=%s", cmd);
        publishRxError_(MqttTopics::SuffixAck, ErrorCode::PolicyDenied, "mqtt.policy", false);
        return;
    }

    const char* argsJson = nullptr;
    char argsBuf[Limits::Mqtt::Buffers::CmdArgs] = {0};
    JsonVariantConst argsVar = root["args"];
    if (!argsVar.isNull()) {
        const size_t written = serializeJson(argsVar, argsBuf, sizeof(argsBuf));
        if (written == 0U || written >= sizeof(argsBuf)) {
            publishRxError_(MqttTopics::SuffixAck, ErrorCode::ArgsTooLarge, "cmd", true);
            return;
        }
        argsJson = argsBuf;
    }

    LOGI("mqtt command accepted cmd=%s", cmd);
    const bool ok = cmdSvc_->execute(
        cmdSvc_->ctx, cmd, msg.payload, argsJson, scratch_->reply, sizeof(scratch_->reply)
    );
    if (!ok) {
        publishRxError_(MqttTopics::SuffixAck, ErrorCode::CmdHandlerFailed, "cmd", false);
        return;
    }
    BufferUsageTracker::note(TrackedBufferId::MqttReplyBuf,
                             strnlen(scratch_->reply, sizeof(scratch_->reply)),
                             sizeof(scratch_->reply),
                             cmd,
                             nullptr);

    const int wrote = snprintf(scratch_->payload, sizeof(scratch_->payload),
                               "{\"ok\":true,\"cmd\":\"%s\",\"reply\":%s}",
                               cmd,
                               scratch_->reply);
    if (!(wrote > 0 && (size_t)wrote < sizeof(scratch_->payload))) {
        publishRxError_(MqttTopics::SuffixAck, ErrorCode::InternalAckOverflow, "cmd", false);
        return;
    }
    BufferUsageTracker::note(TrackedBufferId::MqttPayloadBuf,
                             (size_t)wrote,
                             sizeof(scratch_->payload),
                             cmd,
                             nullptr);

    (void)enqueueAck_(MqttTopics::SuffixAck, scratch_->payload, 0, false, MqttPublishPriority::High);
}

void MQTTModule::processRxCfgSet_(const RxMsg& msg)
{
    if (!scratch_) return;

    if (!cfgSvc_ || !cfgSvc_->applyJson) {
        publishRxError_(MqttTopics::SuffixCfgAck, ErrorCode::CfgServiceUnavailable, "cfg/set", false);
        return;
    }

    static constexpr size_t CFG_DOC_CAPACITY = Limits::JsonCfgBuf;
    static StaticJsonDocument<CFG_DOC_CAPACITY> cfgDoc;
    cfgDoc.clear();

    const DeserializationError cfgErr = deserializeJson(cfgDoc, msg.payload);
    if (cfgErr || !cfgDoc.is<JsonObjectConst>()) {
        BufferUsageTracker::note(TrackedBufferId::MqttCfgDoc,
                                 cfgDoc.memoryUsage(),
                                 sizeof(cfgDoc),
                                 "cfg/set",
                                 nullptr);
        publishRxError_(MqttTopics::SuffixCfgAck, ErrorCode::BadCfgJson, "cfg/set", true);
        return;
    }
    const char* cfgPeakSource = "cfg/set";
    JsonObjectConst cfgRoot = cfgDoc.as<JsonObjectConst>();
    for (JsonPairConst kv : cfgRoot) {
        cfgPeakSource = kv.key().c_str();
        break;
    }
    LOGI("mqtt cfg/set accepted");
    BufferUsageTracker::note(TrackedBufferId::MqttCfgDoc,
                             cfgDoc.memoryUsage(),
                             sizeof(cfgDoc),
                             cfgPeakSource,
                             "<json>");

    if (!cfgSvc_->applyJson(cfgSvc_->ctx, msg.payload)) {
        publishRxError_(MqttTopics::SuffixCfgAck, ErrorCode::CfgApplyFailed, "cfg/set", false);
        return;
    }

    if (!writeOkJson(scratch_->payload, sizeof(scratch_->payload), "cfg/set")) {
        snprintf(scratch_->payload, sizeof(scratch_->payload), "{\"ok\":true}");
    }
    BufferUsageTracker::note(TrackedBufferId::MqttPayloadBuf,
                             strnlen(scratch_->payload, sizeof(scratch_->payload)),
                             sizeof(scratch_->payload),
                             "cfg/set",
                             nullptr);
    (void)enqueueAck_(MqttTopics::SuffixCfgAck, scratch_->payload, 1, false, MqttPublishPriority::High);
}

void MQTTModule::publishRxError_(const char* ackTopicSuffix, ErrorCode code, const char* where, bool parseFailure)
{
    if (!scratch_) return;

    if (parseFailure) ++parseFailCount_;
    else ++handlerFailCount_;

    LOGW("mqtt inbound rejected code=%s where=%s",
         errorCodeStr(code),
         (where && where[0] != '\0') ? where : "unknown");
    syncRxMetrics_();

    if (!writeErrorJson(scratch_->payload, sizeof(scratch_->payload), code, where)) {
        snprintf(scratch_->payload, sizeof(scratch_->payload), "{\"ok\":false}");
    }
    BufferUsageTracker::note(TrackedBufferId::MqttPayloadBuf,
                             strnlen(scratch_->payload, sizeof(scratch_->payload)),
                             sizeof(scratch_->payload),
                             where,
                             nullptr);

    (void)enqueueAck_(ackTopicSuffix, scratch_->payload, 0, false, MqttPublishPriority::High);
}

void MQTTModule::syncRxMetrics_()
{
    if (!dataStore_) return;
    setMqttRxDrop(*dataStore_, rxDropCount_);
    setMqttOversizeDrop(*dataStore_, oversizeDropCount_);
    setMqttParseFail(*dataStore_, parseFailCount_);
    setMqttHandlerFail(*dataStore_, handlerFailCount_);
}

void MQTTModule::countRxDrop_()
{
    ++rxDropCount_;
    syncRxMetrics_();
}

void MQTTModule::countOversizeDrop_()
{
    ++oversizeDropCount_;
    ++rxDropCount_;
    syncRxMetrics_();
}
