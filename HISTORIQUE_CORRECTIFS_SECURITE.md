# Historique des correctifs de securite

Ce document conserve la trace du premier lot de corrections issu de l'audit
du kit Flow.io Waveshare Qwiic. Ces corrections sont reprises dans la version
actuelle du projet.

## Corrections appliquees

- `ESPAsyncWebServer` mis a jour vers `3.11.2`.
- `AsyncTCP` mis a jour vers `3.4.10`.
- `ArduinoJson` mis a jour vers `6.21.6`.
- Jeton CSRF aleatoire de 128 bits et controle des requetes mutantes.
- Validation stricte de l'origine WebSocket `/wslog`.
- En-tetes CSP, anti-clickjacking, anti-MIME-sniffing et isolation des ressources.
- Echappement et troncature sure des reponses JSON de configuration.
- MQTT TLS avec verification du certificat active par defaut.
- Transport d'ecran distant HMI UDP, protocole associe et port 42110 supprimes.
- Limitation Digest portee a 32 sources, avec blocage par IP et plafond global.
- Verification automatique de l'absence du transport HMI UDP dans les sources.

## Validation

- Compilation PlatformIO `Waveshare-ESP32-S3` : reussie.
- Construction SPIFFS : reussie.
- `scripts/verify_release.py` : `release verification: OK`.
- Recherche des marqueurs HMI UDP dans les binaires : aucun resultat.

Les anciens binaires de ce lot ont ete retires du paquet actuel. Le fichier
`binary/manifest.json` ne reference que les artefacts encore distribues.

## Compatibilite API

Les lectures HTTP restent inchangees. Un client qui effectue une action doit :

1. lire `/api/web/meta` avec l'authentification Digest ;
2. recuperer `csrf_token` ;
3. envoyer sa valeur dans `X-Flow-CSRF`.

Le Nextion local reste connecte en UART. Home Assistant continue d'utiliser
MQTT et ne depend pas du transport HMI supprime.

## Risques restant hors de ce lot

- L'administration Web reste en HTTP sans confidentialite de transport.
- Secure Boot v2, chiffrement flash/NVS et anti-rollback restent a industrialiser.
- Le socle Arduino-ESP32 2.0.17 reste a migrer vers une branche maintenue.

Le port 80 ne doit pas etre expose directement a Internet. Pour un acces Web
distant, utiliser un VPN ou un reverse proxy HTTPS de confiance.
