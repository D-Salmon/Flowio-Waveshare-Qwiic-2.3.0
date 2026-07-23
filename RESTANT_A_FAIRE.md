# Travail restant avant production

## Etat actuel

La version `2.3.0` est une edition durcie et allegee consacree au controleur
Waveshare ESP32-S3 N16R8 avec bus Qwiic. Elle contient un seul profil PlatformIO,
ainsi que le firmware ESP32-S3, le SPIFFS et le firmware compatible de l'ecran
Nextion.

Cette version convient aux essais sur banc et a un pilote controle, administre
par USB sur un reseau local isole. Elle ne doit pas etre exposee directement a
Internet.

L'acces nomade doit passer par Home Assistant ou un VPN. Le serveur embarque
reste en HTTP sans TLS et ne doit pas faire l'objet d'une redirection de port
depuis Internet.

## Priorite immediate

1. Valider la version sur le materiel reel:
   - demarrage et recuperation des identifiants sur la console serie;
   - cavalier `GPIO21`-`GND`, AP `FlowIO-RECOVERY`, remplacement des acces,
     extinction effective des huit relais et fermeture apres dix minutes;
   - Wi-Fi, Ethernet et MQTT TLS;
   - interface Web et SPIFFS;
   - ecran Nextion et liaison HMI;
   - bus Qwiic, DS2484, ADS1115 et sondes;
   - huit relais, retours de contacteurs et securites filtration/electrolyseur;
   - redemarrage et persistance de la configuration.
2. Documenter la procedure de mise en service, de sauvegarde et de restauration
   pour les installateurs.

## Securite avant production

1. Creer sur le broker un compte propre a chaque appareil, interdire les
   connexions anonymes et appliquer les ACL de `docs/mqtt-hardening.md`.
   Le firmware refuse deja les identifiants vides, limite les rafales et
   interdit par MQTT les mises a jour et imports de configuration.
2. Remplacer l'OTA non signee actuellement desactivee par une OTA HTTPS signee,
   avec verification cryptographique, rollback et protection anti-downgrade.
3. Preparer Secure Boot v2, le chiffrement flash/NVS et une procedure de
   programmation controlee des eFuses pour la fabrication.
4. Signer le manifeste de livraison: les sommes SHA-256 actuelles assurent
   l'integrite, mais pas l'authenticite.

## Industrialisation et maintenance

1. Ajouter une integration continue qui compile le profil
   `Waveshare-ESP32-S3`, construit le SPIFFS et execute la verification de
   release.
2. Ajouter des tests d'integration pour l'authentification Web, la rotation des
   identifiants, MQTT TLS et les coupures pendant une mise a
   jour.
3. Verrouiller les dependances transitives et produire un SBOM CycloneDX ou
   SPDX avec les licences tierces.
4. Rendre les builds reproductibles avec `SOURCE_DATE_EPOCH`, l'identifiant du
   commit et la version exacte de la chaine d'outils.
5. Definir et ajouter la licence du projet.

## Ordre recommande

1. Essais materiels complets.
2. Compte et ACL MQTT sur le broker, puis essai de la limitation firmware.
3. OTA signee.
4. Secure Boot, chiffrement et procedure de fabrication.
