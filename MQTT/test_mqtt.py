import paho.mqtt.client as client
import time

BROKER_IP = "172.20.10.7"
BROKER_PORT = 1883
TOPIC_TEST = "B3/228/test"

cli = client.Client()

# --- Callbacks ---

def fctReceptionMsg(ud, c, m):
    # m.payload est en bytes => on décode pour afficher proprement
    try:
        payload_str = m.payload.decode("utf-8")
    except:
        payload_str = str(m.payload)
    print(f"[GEN] {m.topic} : {payload_str}")

def fctTopicTemperature(ud, c, m):
    print(f"[TEMP] {m.payload.decode('utf-8', errors='ignore')}")

def fctTopicImage(ud, c, m):
    print("[IMAGE] Image reçue")

# --- Connexion et config ---

cli.connect(BROKER_IP, BROKER_PORT)

# Abonnement (tous les topics pour tester)
cli.subscribe("#")

# Callbacks spécifiques
cli.message_callback_add("Image", fctTopicImage)
cli.message_callback_add("Temperature", fctTopicTemperature)

# Callback général
cli.on_message = fctReceptionMsg

# Lancement de la boucle réseau
cli.loop_start()

# --- Boucle d'envoi périodique ---

i = 0
try:
    while True:
        message = f"Mon i vaut {i}"
        print(f"[PY] Publication sur {TOPIC_TEST} : {message}")
        cli.publish(TOPIC_TEST, message)
        i += 1
        time.sleep(5)
except KeyboardInterrupt:
    print("Arrêt demandé par l'utilisateur.")
finally:
    cli.loop_stop()
    cli.disconnect()
