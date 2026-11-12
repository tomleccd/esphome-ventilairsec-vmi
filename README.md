# ESPHome — VMI Ventilairsec Pulse'R

Pilotez une VMI **Ventilairsec Pulse'R** (Cube / Urban) depuis Home Assistant, avec un ESP32
et un module radio CC1101 à 868 MHz. L'ESP parle à la VMI comme la télécommande d'origine,
qui continue de fonctionner normalement.

*Control a Ventilairsec Pulse'R positive input ventilation unit from Home Assistant with an
ESP32 and a CC1101 868 MHz radio. The radio protocol is documented in [PROTOCOL.md](PROTOCOL.md).*

> **Projet indépendant, non affilié à Ventilairsec.** Testé sur une seule installation.
> L'appareil n'est pas modifié, mais l'ESP pilote une résistance de chauffage : utilisation à vos risques.

## Ce que vous obtenez dans Home Assistant

**Commandes**
- Vitesse (1 à 10), pilotage manuel / automatique
- Préchauffage : arrêt, 12, 15 ou 18 °C
- Boost 30 min, vacances, surventilation été, turbochauffe hiver
- Température max (seuil de mise en veille), 20 à 45 °C

**Mesures relevées sur la VMI** (actualisées chaque minute)
- Vitesse réelle, consigne de chauffage réelle, pilotage réel, modes actifs
- Température et humidité dans la VMI, puissance de chauffage
- Jours d'utilisation du filtre, durée de fonctionnement, température max
- Code d'état (64 = normal), utile pour une alerte de défaut

**Relevées sur la télécommande** : température et humidité de la pièce (à chaque appui et toutes les 10 minutes).

**Remplacer la télécommande pour la température et l'humidité** (optionnel) : l'ESP peut envoyer
à la VMI les mesures d'une sonde Home Assistant à la place de celles de la télécommande. Utile en
pilotage automatique, quand la télécommande n'est pas dans la pièce de vie. Indiquez la sonde dans
les substitutions `room_temp_entity` et `room_hum_entity`, puis activez l'interrupteur
« Remplacer la telecommande (temp hum) ».

## Matériel

- Un ESP32-S3 (testé sur une carte type *ESP32-S3-DevKitC-1*)
- Un module **CC1101 868 MHz** (attention, pas la version 433 MHz)
- Placez l'ESP à quelques mètres de la VMI : la VMI émet faiblement.

| CC1101 | ESP32-S3 |
|--------|----------|
| VCC    | 3V3      |
| GND    | GND      |
| MOSI   | GPIO6    |
| SCLK   | GPIO12   |
| MISO   | GPIO13   |
| CSN    | GPIO10   |
| GDO0   | GPIO4    |
| GDO2   | non utilisé |

Les broches se changent dans les `substitutions` en tête du fichier.

## Installation

1. Copiez `firmware/vmi-ventilairsec.yaml` **et** `firmware/vmi_rf.h` dans votre dossier ESPHome.
2. Ajoutez à `secrets.yaml` : `wifi_ssid`, `wifi_password`, `vmi_api_key` (clé de chiffrement
   API en base64) et `vmi_ota_password`.
3. Compilez et flashez (ESPHome **2026.10 ou plus récent**, pour le composant `cc1101` natif).
4. Ajoutez l'appareil dans Home Assistant.

## Apprentissage (une seule fois)

L'ESP ne connaît pas votre installation au départ et n'émet rien tant qu'il n'a pas appris.

1. Dans Home Assistant, activez **« Apprentissage telecommande »** (il se coupe seul après 10 min).
2. Placez la télécommande à moins d'un mètre de l'ESP et réveillez-la : l'ESP apprend
   l'identifiant de votre télécommande, puis les trames de réveil, de lecture et de vitesse.
3. Sur l'écran des options, activez puis désactivez un mode (le boost par exemple).
4. Sur l'écran des consignes de température, changez la température max puis remettez-la.
5. Le capteur **« Apprentissage etat »** doit afficher **« Complet »**. Sinon, il indique ce qui manque.

L'ESP ne lit et ne pilote que la VMI dont il a appris l'identifiant ; les trames des voisins sont ignorées.

## Bon à savoir

- **Manuel / automatique** : à chaque réveil, la télécommande renvoie son propre réglage et
  écrase celui de Home Assistant. Réglez-la une fois sur le mode que vous voulez garder.
- **Boost** : s'arrête seul après 30 minutes, comme depuis la télécommande.
- **Fréquence** : si rien n'est reçu, votre module CC1101 est peut-être décalé. Ajustez
  `frequency` par pas de 20 kHz autour de 868,04 MHz.
- **Mode automatique** : la VMI régule avec la température et l'humidité de la pièce. Sans sonde
  Home Assistant, ce sont celles de la télécommande, là où elle est posée.
- **Consommation** (mesurée sur une installation, ventilateur seul) : environ 22 W en vitesse 3,
  32 W en 5, 38 W en 6, 41 W en 7.

## Limites connues

- Les commandes sont construites à partir des trames apprises de votre télécommande et de tables
  mesurées sur une seule installation (voir [PROTOCOL.md](PROTOCOL.md)).
- Pas encore de remise à zéro du compteur de filtre, ni des indicateurs veille et été.

Les retours sur d'autres installations (modèle, versions affichées dans le menu installateur,
ce qui marche ou non) sont les bienvenus dans les *issues*.

## Licence

MIT. Ventilairsec et Pulse'R sont des marques de leurs propriétaires, citées uniquement pour
indiquer la compatibilité.
