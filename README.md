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

Batterie + charge possible via panneau solaire

Deep-sleep ESP32 pour limiter la consommation

Réveil via PIR ou timer RTC






🚀 Feuillete de Route du Projet Nichet Connecté
Ce document liste les fonctionnalités et améliorations prévues pour les prochaines itérations du projet.

🔥 Priorité Haute (Core Functionality)
Ces éléments sont essentiels pour la première version opérationnelle et fiable.



Statut,Tâche,Description
[ ],Détection de Mouvement,Ajouter un capteur PIR (Passive Infrared) pour déclencher les actions (prise de photo) uniquement en cas de mouvement.
[ ],Gestion de l'Alimentation Profonde,Implémenter le mode deep-sleep de l'ESP32 avec réveil par RTC ou interruption PIR pour une économie d'énergie maximale.
[ ],Résilience Réseau,Mettre en place des mécanismes de retry et une file d'attente (queue) pour gérer les problèmes de connexion réseau ou MQTT temporaires.
[ ],Topic Diagnostic (status),"Ajouter un topic MQTT nichoir/status pour envoyer des informations de diagnostic de l'ESP32 (mémoire, uptime, état WiFi, etc.)."
[ ],Topic Commande (cmd),"Ajouter un topic MQTT nichoir/cmd pour des commandes à distance, comme la prise de photo forcée."



