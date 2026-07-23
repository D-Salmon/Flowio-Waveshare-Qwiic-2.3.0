# Signature des mises a jour OTA

Les televersements locaux de firmware et de SPIFFS utilisent une signature
ECDSA P-256 sur le SHA-256 exact du fichier `.bin`. Une image non signee ou
signee avec une autre cle est abandonnee avant l'activation de la partition.

## 1. Creer les cles hors du depot

Sur une machine de livraison protegee :

```sh
openssl ecparam -name prime256v1 -genkey -noout -out flowio-ota-private.pem
openssl ec -in flowio-ota-private.pem -pubout -out flowio-ota-public.pem
```

La cle privee ne doit jamais etre copiee dans ce depot, dans le firmware ou sur
l'appareil. La conserver dans un coffre CI, un HSM ou un support hors ligne.

## 2. Provisionner la cle publique

Copier le PEM public dans `include/Security/OtaPublicKey.h`, sous forme de chaine
multi-ligne C++. Tant que cette valeur est vide, l'OTA signee echoue de maniere
fermee avec `Cle publique OTA non provisionnee`.

## 3. Signer un artefact

```sh
python scripts/sign_ota.py binary/esp32s3-2.2.0.bin \
  --private-key /chemin/protege/flowio-ota-private.pem
```

Le script produit `esp32s3-2.2.0.bin.sig`, qui contient la signature DER encodee
en Base64. Signer separement l'image SPIFFS.

## 4. Installer

Dans l'interface Web, selectionner le fichier `.bin` et son fichier `.sig`, puis
lancer l'installation. L'authentification Web, le CSRF, le SHA-256 et la
signature ECDSA sont tous verifies.

## Limites actuelles

- La verification signee est active pour les televersements locaux Waveshare.
- Les mises a jour reseau restent desactivees par `FLOW_ALLOW_UNSIGNED_UPDATES=0`
  jusqu'a l'ajout d'un manifeste signe et d'un transport HTTPS valide.
- La protection anti-retour de version et Secure Boot/eFuses restent a ajouter.
