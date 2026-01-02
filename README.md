# SmartCities_Projet_THERER_ROBERTY


🐦 Présentation

Ce projet vise à créer un nichoir intelligent capable de :

Capturer automatiquement des images lorsqu’un oiseau entre dans le nichoir

Envoyer ces images via MQTT vers un serveur Raspberry Pi

Fonctionner de manière autonome, avec optimisation de la consommation

Permettre l'analyse et le suivi d’activité des oiseaux.

Le système repose sur un ESP32 TimerCAM, un Raspberry Pi avec Mosquitto, et un script de réception d’images.


Module ESP32 (TimerCAM)

Capture d’image JPEG via la bibliothèque custom MyTimerCam

Envoi direct du buffer JPEG sur le topic MQTT :

nichoir/image


Résolution configurable (SVGA, UXGA…)

Paramétrage du taux de compression JPEG

Fonctionnement continu ou déclenché (PIR, timer…)


🖥️ Raspberry Pi

Broker MQTT Mosquitto

Script Python utilisant paho-mqtt :

souscrit au topic nichoir/image

reçoit le JPEG complet

enregistre dans images_reçues/

Nommage automatique des fichiers selon timestamp

🔌 Alimentation & Énergie

Batterie

Deep-sleep ESP32 pour limiter la consommation

Réveil via PIR ou timer RTC




