#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import mariadb
from datetime import datetime
import paho.mqtt.client as mqtt

# -------------------------------------------------------------------
# Pour lancer :
#   - mettre le bon Broker_ip (celui du rasp)
#   - se placer sur le Desktop (ou pas, peu importe maintenant)
#   - python3 receiver_save_image.py
# -------------------------------------------------------------------

# -----------------------------
# CONFIG MQTT
# -----------------------------
BROKER_IP   = "192.168.0.101"
BROKER_PORT = 1883
TOPIC_IMAGE = "nichoir/image"

# -----------------------------
# CONFIG DOSSIER IMAGES
# -----------------------------
# Dossier images sur ton Desktop, quel que soit ton user Linux
HOME_DIR = os.path.expanduser("~")
SAVE_DIR = os.path.join(HOME_DIR, "Desktop", "images_reçues")
os.makedirs(SAVE_DIR, exist_ok=True)

# -----------------------------
# CONFIG BASE DE DONNÉES
# -----------------------------
DB_CONFIG = {
    "user": "mat",
    "password": "123456789",
    "host": "localhost",
    "database": "nichoir",
    "port": 3306
}

def insert_image_record(chemin_fichier, niveau_batterie=None, commentaire=None):
    """
    Insère une nouvelle ligne dans la table 'images' de la base 'nichoir'.

    champs : chemin_fichier (str), date_capture (now), niveau_batterie (int ou None),
             commentaire (str ou None)
    """
    try:
        conn = mariadb.connect(
            user=DB_CONFIG["user"],
            password=DB_CONFIG["password"],
            host=DB_CONFIG["host"],
            port=DB_CONFIG["port"],
            database=DB_CONFIG["database"]
        )
        cursor = conn.cursor()

        sql = """
            INSERT INTO images (chemin_fichier, date_capture, niveau_batterie, commentaire)
            VALUES (?, ?, ?, ?)
        """
        valeurs = (
            chemin_fichier,
            datetime.now(),
            niveau_batterie,
            commentaire
        )

        cursor.execute(sql, valeurs)
        conn.commit()

        try:
            new_id = cursor.lastrowid
            print(f"   → Base mise à jour (ID = {new_id})")
        except AttributeError:
            print("   → Base mise à jour (ID non disponible)")

        cursor.close()
        conn.close()

    except mariadb.Error as e:
        print("❌ ERREUR MariaDB :", e)

# -----------------------------
# CALLBACKS MQTT
# -----------------------------
def on_connect(client, userdata, flags, rc):
    print("Connecté au broker avec le code de retour", rc)
    client.subscribe(TOPIC_IMAGE)
    print(f"Abonné au topic : {TOPIC_IMAGE}")

def on_message(client, userdata, msg):
    if msg.topic == TOPIC_IMAGE:
        # Génère un nom unique basé sur la date/heure
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"image_{timestamp}.jpg"
        filepath = os.path.join(SAVE_DIR, filename)

        # Sauvegarder l’image reçue
        try:
            with open(filepath, "wb") as f:
                f.write(msg.payload)
            print(f"[{timestamp}] Image reçue → {filepath}")
        except Exception as e:
            print("❌ ERREUR lors de l’écriture du fichier image :", e)
            return

        # Chemin absolu pour la base
        full_path = os.path.abspath(filepath)

        # Mise à jour DB
        insert_image_record(
            chemin_fichier=full_path,
            niveau_batterie=None,   # tu mettras une vraie valeur plus tard si tu veux
            commentaire="Image auto"
        )

    else:
        print(f"Message sur {msg.topic}: {msg.payload[:30]}...")

# -----------------------------
# LANCEMENT MQTT
# -----------------------------
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

print("Connexion au broker MQTT...")
client.connect(BROKER_IP, BROKER_PORT, keepalive=60)
client.loop_forever()
