# Configuration MQTT et Home Assistant

Ce mémo indique les paramètres à saisir dans l'interface Web locale de
Flow.io après l'installation du firmware et la connexion du Waveshare au
Wi-Fi.

Le broker Mosquitto est installé sur le Raspberry Pi Home Assistant. Le port
TCP 8883 est ouvert sur la box du site Home Assistant et redirigé vers ce
Raspberry Pi. Le compte MQTT dédié `flowio_device` et son ACL ont déjà été
créés.

## 1. Configuration Home Assistant

Dans l'interface Web de Flow.io, ouvrir :

`Configuration` → `Config Store Flow.io` → `ha`

Renseigner :

| Champ | Valeur |
|---|---|
| Home Assistant activé | Oui |
| Identifiant appareil HA | `flowio_device` |
| Préfixe Discovery | `homeassistant` |

Cliquer sur **Appliquer**.

Les autres champs HA, comme le constructeur, le modèle ou le préfixe des
entités, peuvent conserver leurs valeurs par défaut.

## 2. Configuration MQTT

Ouvrir ensuite :

`Configuration` → `Config Store Flow.io` → `mqtt`

Renseigner :

| Champ | Valeur |
|---|---|
| MQTT activé | Oui |
| Hôte MQTT | Le nom de domaine public de Home Assistant, par exemple `ha.xxxxx.fr` |
| Port MQTT | `8883` |
| Utilisateur MQTT | `flowio_device` |
| Mot de passe MQTT | Le mot de passe aléatoire conservé séparément |
| Topic de base | `flowio` |
| ID device MQTT topic | `flowio_device` |
| Nom d'appareil MQTT | `Piscine` |

Cliquer sur **Appliquer**.

Dans le champ **Hôte MQTT**, saisir le nom de domaine utilisé pour accéder au
site Home Assistant, à condition que son certificat TLS couvre bien ce nom et
que le port TCP 8883 soit redirigé vers Mosquitto. Par exemple, si Home
Assistant est accessible avec `https://ha.xxxxx.fr`, saisir uniquement :

```text
ha.xxxxx.fr
```

Ne pas ajouter `https://`, `mqtts://`, `:8883` ou de chemin dans ce champ. Le
port est renseigné séparément dans **Port MQTT**. Ne pas utiliser l'adresse IP
locale du Raspberry Pi lorsque Flow.io se trouve sur un autre réseau.

Le mot de passe MQTT ne doit pas être écrit dans ce fichier ni publié sur
GitHub.

## 3. Vérification

Après l'application des paramètres, la connexion MQTT se relance
automatiquement.

Dans l'interface Flow.io, vérifier que l'état MQTT indique :

```text
État : Connecté
Serveur : ha.xxxxx.fr
```

Dans le journal Mosquitto, une connexion utilisant le compte
`flowio_device` doit apparaître.

Home Assistant devrait ensuite découvrir automatiquement le contrôleur et
ses entités sous MQTT.

## 4. En cas d'échec

Vérifier, dans cet ordre :

1. le nom de domaine `ha.xxxxx.fr` ;
2. le port `8883` ;
3. le nom d'utilisateur `flowio_device` ;
4. le mot de passe MQTT ;
5. les identifiants `flowio_device` dans les branches `ha` et `mqtt` ;
6. l'état du port TLS 8883 et le journal Mosquitto ;
7. la validité du certificat TLS associé au nom de domaine.

Ne pas ouvrir les ports MQTT `1883`, `1884` ou `8884`, ni l'interface Web de
l'ESP32, pour cette installation.
