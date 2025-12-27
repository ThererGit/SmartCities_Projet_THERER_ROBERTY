#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import mariadb
from datetime import datetime
import paho.mqtt.client as mqtt

# -----------------------------
# CONFIG MQTT
# -----------------------------
BROKER_IP   = "192.168.0.101"
BROKER_PORT = 1883
TOPIC_IMAGE = "nichoir/image"

# -----------------------------
# CONFIG DOSSIER IMAGES
# -----------------------------
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

# -----------------------------
# FONCTION DE NETTOYAGE (SYNC)
# -----------------------------
def sync_database_with_folder():
    """
    Vérifie au démarrage si les fichiers en base existent vraiment sur le disque.
    Si un fichier a été supprimé manuellement, on le retire de la base.
    """
    print("--- 🧹 Vérification de l'intégrité de la base de données ---")
    try:
        conn = mariadb.connect(**DB_CONFIG)
        cursor = conn.cursor()

        # 1. On récupère toutes les entrées
        cursor.execute("SELECT id, chemin_fichier FROM images")
        rows = cursor.fetchall()
        
        deleted_count = 0

        # 2. On boucle sur chaque ligne pour vérifier le fichier
        for row in rows:
            image_id = row[0]
            chemin = row[1]

            if not os.path.exists(chemin):
                # Le fichier n'existe plus sur le disque !
                print(f"   ⚠️ Fichier introuvable : {chemin}")
                print(f"      -> Suppression de l'entrée ID {image_id}...")
                
                # Suppression dans la BDD
                cursor.execute("DELETE FROM images WHERE id = ?", (image_id,))
                deleted_count += 1
        
        # 3. On valide les suppressions
        if deleted_count > 0:
            conn.commit()
            print(f"✅ Nettoyage terminé : {deleted_count} images supprimées de la base.")
        else:
            print("✅ Tout est en ordre : La base correspond aux fichiers.")

        cursor.close()
        conn.close()

    except mariadb.Error as e:
        print("❌ ERREUR lors de la synchronisation BDD :", e)
    print("----------------------------------------------------------")


def insert_image_record(chemin_fichier, niveau_batterie=None, commentaire=None):
    """ Insère une nouvelle ligne dans la table 'images'. """
    try:
        conn = mariadb.connect(**DB_CONFIG)
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
            print("   → Base mise à jour")

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
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"image_{timestamp}.jpg"
        filepath = os.path.join(SAVE_DIR, filename)

        try:
            with open(filepath, "wb") as f:
                f.write(msg.payload)
            print(f"[{timestamp}] Image reçue → {filepath}")
        except Exception as e:
            print("❌ ERREUR lors de l’écriture du fichier image :", e)
            return

        full_path = os.path.abspath(filepath)

        insert_image_record(
            chemin_fichier=full_path,
            niveau_batterie=None,
            commentaire="Image auto"
        )

    else:
        print(f"Message sur {msg.topic}: {msg.payload[:30]}...")

# -----------------------------
# LANCEMENT PRINCIPAL
# -----------------------------
if __name__ == "__main__":
    
    # 1. D'abord on nettoie la BDD (Sync)
    sync_database_with_folder()

    # 2. Ensuite on lance MQTT
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    print("Connexion au broker MQTT...")
    try:
        client.connect(BROKER_IP, BROKER_PORT, keepalive=60)
        client.loop_forever()
    except Exception as e:
        print(f"❌ Impossible de se connecter au broker MQTT ({BROKER_IP}) : {e}")