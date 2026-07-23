# Documentation Waveshare ESP32-S3

Ce paquet contient uniquement le firmware autonome Waveshare Qwiic. Les
documents propres aux anciens firmwares FlowIO classique, Supervisor et
Micronova ne font pas partie de cette edition.

## Demarrage

- [Mise en service et flash](integration/mise-en-service.md)
- [Durcissement de securite](security-hardening.md)

## Architecture

- [Architecture generale](core/architecture.md)
- [Profils, cartes, domaines et bootstrap](core/profiles-board-domain-app.md)
- [Services Core](core/services.md)
- [Modele de donnees et evenements](core/data-event-model.md)
- [Topologie MQTT](core/mqtt-topics.md)
- [Exposition Runtime UI](core/runtime-ui-exposure.md)
- [Empreinte memoire](core/memory-footprint-flowio.md)
- [Qualite des modules](core/module-quality-gates.md)

## Modules inclus

- [AlarmModule](modules/AlarmModule.md)
- [CommandModule](modules/CommandModule.md)
- [ConfigStoreModule](modules/ConfigStoreModule.md)
- [DataStoreModule](modules/DataStoreModule.md)
- [EventBusModule](modules/EventBusModule.md)
- [HAModule](modules/HAModule.md)
- [HMIModule](modules/HMIModule.md)
- [IOModule](modules/IOModule.md)
- [LogAlarmSinkModule](modules/LogAlarmSinkModule.md)
- [LogDispatcherModule](modules/LogDispatcherModule.md)
- [LogHubModule](modules/LogHubModule.md)
- [LogSerialSinkModule](modules/LogSerialSinkModule.md)
- [MQTTModule](modules/MQTTModule.md)
- [PoolDeviceModule](modules/PoolDeviceModule.md)
- [PoolLogicModule](modules/PoolLogicModule.md)
- [SystemModule](modules/SystemModule.md)
- [SystemMonitorModule](modules/SystemMonitorModule.md)
- [TimeModule](modules/TimeModule.md)
- [WifiModule](modules/WifiModule.md)
