# Durcissement MQTT retenu

Ce profil conserve les commandes Home Assistant et les correctifs `cfg/set`,
mais impose une authentification MQTT, limite les rafales et refuse les actions
de mise a jour ou d'import complet de configuration recues par MQTT.

## Configuration du broker

Les trois premieres mesures sont a appliquer dans Mosquitto; le firmware ne
peut pas imposer la politique d'un broker distant.

1. Creer un utilisateur propre a chaque controleur, par exemple
   `flowio_ESP32_A1B2C3`, avec un mot de passe aleatoire long et unique.
2. Interdire les connexions anonymes (`allow_anonymous false`) sur le listener
   utilise par Flow.io.
3. Limiter ce compte aux topics du controleur et a son propre noeud de
   decouverte Home Assistant.

Exemple de principe pour un appareil dont `baseTopic=flowio`,
`topicDeviceId=ESP32-A1B2C3` et dont le noeud de decouverte HA vaut
`0xaabbccddeeff`:

```text
user flowio_ESP32_A1B2C3
topic readwrite flowio/ESP32-A1B2C3/#
topic write homeassistant/+/0xaabbccddeeff/+/config
```

Les valeurs doivent etre remplacees par celles de l'appareil. Le noeud HA est
le `ha/device_id` normalise; lorsqu'il est vide, il est derive de l'adresse MAC.
Le compte Home Assistant reste distinct et doit naturellement pouvoir publier
les commandes dans `flowio/ESP32-A1B2C3/#`.

Apres activation des ACL, verifier:

- publication de `status`, `rt/*`, `cfg/*` et `ack`;
- creation des entites sous `homeassistant/.../config`;
- commandes Home Assistant vers les relais et reglages;
- refus d'une lecture ou ecriture vers le topic d'un autre appareil.

## Controles effectues par le firmware

Avec `FLOW_MQTT_REQUIRE_AUTH=1`, une configuration sans utilisateur ou sans mot
de passe ne lance aucune connexion au broker. TLS reste obligatoire separement
avec `FLOW_MQTT_REQUIRE_TLS=1`.

Le recepteur accepte au maximum 12 messages en 10 secondes. Le message suivant
declenche un blocage de 60 secondes. Les commandes acceptees et refusees sont
journalisees par leur nom ou leur topic, jamais avec leur payload ni un secret.
Les refus repetes pendant le blocage ne sont rapportes qu'une fois toutes les
cinq secondes.

Les commandes suivantes sont refusees lorsqu'elles arrivent par MQTT:

- toutes les commandes `fw.update.*` qui lancent une mise a jour;
- `config.import` et `config.restore`;
- `cfg.import` et `cfg.restore`.

`fw.update.status` reste autorisee car elle est en lecture seule. Restent aussi
autorises, conformement au perimetre retenu:

- `cfg/set`;
- les commandes Home Assistant;
- `system.reboot` et `system.factory_reset`;
- les commandes de maintenance qui ne lancent ni mise a jour ni import.

Le blocage concerne uniquement l'origine MQTT. L'administration Web conserve
ses propres controles d'authentification, CSRF et signature OTA.
